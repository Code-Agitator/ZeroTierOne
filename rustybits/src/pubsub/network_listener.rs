use crate::pubsub::change_listener::ChangeListener;
use crate::pubsub::protobuf::pbmessages::NetworkChange;
use prost::Message;
use serde_json;
use std::io::Write;
use std::os::raw::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::mpsc::Receiver;
use tokio::sync::Mutex;

pub type NetworkListenerCallback = extern "C" fn(*mut c_void, *const u8, usize);

/**
 * Network Listener listens for network changes and passes them back to the controller
 *
 * This is a wrapper around ChangeListener that specifically handles network changes.
 * It uses a Tokio channel to receive messages and decodes them into NetworkChange messages.
 */
pub struct NetworkListener {
    change_listener: ChangeListener,
    rx_channel: Mutex<Receiver<Vec<u8>>>,
    callback: Mutex<NetworkListenerCallback>,
    user_ptr: AtomicPtr<c_void>,
}

impl NetworkListener {
    pub async fn new(
        controller_id: &str,
        listen_timeout: Duration,
        callback: NetworkListenerCallback,
        user_ptr: *mut c_void,
    ) -> Result<Arc<Self>, Box<dyn std::error::Error>> {
        let (tx, rx) = tokio::sync::mpsc::channel(64);

        let change_listener = ChangeListener::new(
            controller_id,
            "controller-network-change-stream",
            format!("{}-network-change-subscription", controller_id).as_str(),
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

    pub async fn listen(&self) -> Result<(), Box<dyn std::error::Error>> {
        self.change_listener.listen().await
    }

    pub fn change_handler(self: Arc<Self>) -> Result<(), Box<dyn std::error::Error>> {
        let this = self.clone();
        tokio::spawn(async move {
            let mut rx = this.rx_channel.lock().await;
            while let Some(change) = rx.recv().await {
                if let Ok(m) = NetworkChange::decode(change.as_slice()) {
                    print!("Received change: {:?}", m);

                    let j = serde_json::to_string(&m).unwrap();
                    let mut buffer = [0; 16384];
                    let mut test: &mut [u8] = &mut buffer;
                    let mut size: usize = 0;
                    while let Ok(bytes) = test.write(j.as_bytes()) {
                        if bytes == 0 {
                            break; // No more space to write
                        }
                        size += bytes;
                    }
                    let callback = this.callback.lock().await;
                    let user_ptr = this.user_ptr.load(Ordering::Relaxed);

                    (callback)(user_ptr, test.as_ptr(), size);
                }
            }
        });

        Ok(())
    }
}
