use crate::pubsub::change_listener::ChangeListener;
use crate::pubsub::protobuf::pbmessages::MemberChange;
use prost::Message;
use std::io::Write;
use std::os::raw::c_void;
use std::sync::atomic::AtomicPtr;
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::mpsc::Receiver;
use tokio::sync::Mutex;

pub type MemberListenerCallback = extern "C" fn(*mut c_void, *const u8, usize);

/**
 * Member Listener listens for member changes and passes them back to the controller
 *
 * This is a wrapper around ChangeListener that specifically handles member changes.
 * It uses a Tokio channel to receive messages and decodes them into MemberChange messages.
 */
pub struct MemberListener {
    change_listener: ChangeListener,
    rx_channel: Mutex<Receiver<Vec<u8>>>,
    callback: Mutex<MemberListenerCallback>,
    user_ptr: AtomicPtr<c_void>,
}

impl MemberListener {
    pub async fn new(
        controller_id: &str,
        listen_timeout: Duration,
        callback: MemberListenerCallback,
        user_ptr: *mut c_void,
    ) -> Result<Arc<Self>, Box<dyn std::error::Error>> {
        let (tx, rx) = tokio::sync::mpsc::channel(64);

        let change_listener = ChangeListener::new(
            controller_id,
            "controller-member-change-stream",
            format!("{}-member-change-subscription", controller_id).as_str(),
            listen_timeout,
            tx,
        )
        .await?;

        Ok(Arc::new(Self {
            change_listener,
            rx_channel: Mutex::new(rx),
            callback: Mutex::new(callback),
            user_ptr: AtomicPtr::new(user_ptr as *mut c_void),
        }))
    }

    pub async fn listen(self: &Arc<Self>) -> Result<(), Box<dyn std::error::Error>> {
        self.change_listener.listen().await
    }

    pub async fn change_handler(self: &Arc<Self>) -> Result<(), Box<dyn std::error::Error>> {
        let this = self.clone();

        let mut rx = this.rx_channel.lock().await;
        while let Some(change) = rx.recv().await {
            if let Ok(m) = MemberChange::decode(change.as_slice()) {
                let j = serde_json::to_string(&m).unwrap();
                let mut buffer = [0; 16384];
                let mut test: &mut [u8] = &mut buffer;
                let mut size: usize = 0;
                while let Ok(bytes) = test.write(j.as_bytes()) {
                    if bytes == 0 {
                        break;
                    }
                    size += bytes;
                }
                let callback = this.callback.lock().await;
                let user_ptr = this.user_ptr.load(std::sync::atomic::Ordering::Relaxed);

                (callback)(user_ptr, test.as_ptr(), size);
            } else {
                eprintln!("Failed to decode change");
            }
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::pubsub::change_listener::tests::setup_pubsub_emulator;
    use crate::pubsub::protobuf::pbmessages::member_change::Member;
    use crate::pubsub::protobuf::pbmessages::MemberChange;

    use gcloud_googleapis::pubsub::v1::PubsubMessage;
    use gcloud_pubsub::client::{Client, ClientConfig};
    use std::{
        collections::HashMap,
        sync::atomic::{AtomicBool, Ordering},
    };

    extern "C" fn dummy_callback(user_ptr: *mut c_void, data: *const u8, _size: usize) {
        // Dummy callback for testing
        assert!(!data.is_null(), "data pointer is null");
        assert!(!user_ptr.is_null(), "user_ptr pointer is null");
        let user_ptr = unsafe { &mut *(user_ptr as *mut TestMemberListener) };
        user_ptr.callback_called();
        println!("Dummy callback invoked");
    }

    struct TestMemberListener {
        dummy_callback_called: bool,
    }

    impl TestMemberListener {
        fn new() -> Self {
            Self { dummy_callback_called: false }
        }

        fn callback_called(&mut self) {
            self.dummy_callback_called = true;
        }
    }

    #[tokio::test(flavor = "multi_thread", worker_threads = 2)]
    async fn test_member_listener() {
        println!("Setting up Pub/Sub emulator for network listener test");
        let (_container, _host) = setup_pubsub_emulator().await.unwrap();
        let mut tester = TestMemberListener::new();

        let listener = MemberListener::new(
            "testctl",
            Duration::from_secs(1),
            dummy_callback,
            &mut tester as *mut TestMemberListener as *mut c_void,
        )
        .await
        .unwrap();

        let rt = tokio::runtime::Handle::current();

        let run = Arc::new(AtomicBool::new(true));
        rt.spawn({
            let run = run.clone();
            let l = listener.clone();
            async move {
                while run.load(Ordering::Relaxed) {
                    match l.listen().await {
                        Ok(_) => {
                            println!("Listener exited successfully");
                        }
                        Err(e) => {
                            println!("Failed to start listener: {}", e);
                            assert!(false, "Listener failed to start");
                        }
                    }
                }
            }
        });

        rt.spawn({
            let run = run.clone();
            let l = listener.clone();
            async move {
                while run.load(Ordering::Relaxed) {
                    match l.change_handler().await {
                        Ok(_) => {
                            println!("Change handler started successfully");
                        }
                        Err(e) => {
                            println!("Failed to start change handler: {}", e);
                            assert!(false, "Change handler failed to start");
                        }
                    }
                }
            }
        });

        rt.spawn({
            async move {
                let client = Client::new(ClientConfig::default()).await.unwrap();
                let topic = client.topic("controller-member-change-stream");
                if !topic.exists(None).await.unwrap() {
                    topic.create(None, None).await.unwrap();
                }

                let mut publisher = topic.new_publisher(None);

                let nc = MemberChange {
                    old: Some(Member {
                        device_id: "test_member".to_string(),
                        network_id: "test_network".to_string(),
                        authorized: false,
                        ..Default::default()
                    }),
                    new: Some(Member {
                        device_id: "test_member".to_string(),
                        network_id: "test_network".to_string(),
                        authorized: true,
                        ..Default::default()
                    }),
                    ..Default::default()
                };

                let data = MemberChange::encode_to_vec(&nc);
                let message = PubsubMessage {
                    data: data.into(),
                    attributes: HashMap::from([("controller_id".to_string(), "testctl".to_string())]),
                    ordering_key: format!("members-{}", "test_network"),
                    ..Default::default()
                };
                let awaiter = publisher.publish(message).await;

                match awaiter.get().await {
                    Ok(_) => println!("Message published successfully"),
                    Err(e) => {
                        assert!(false, "Failed to publish message: {}", e);
                        eprintln!("Failed to publish message: {}", e)
                    }
                }
                publisher.shutdown().await;
            }
        });

        let mut counter = 0;
        while !tester.dummy_callback_called && counter < 100 {
            tokio::time::sleep(Duration::from_millis(100)).await;
            counter += 1;
        }
        run.store(false, Ordering::Relaxed);
        assert!(tester.dummy_callback_called, "Callback was not called");
    }
}
