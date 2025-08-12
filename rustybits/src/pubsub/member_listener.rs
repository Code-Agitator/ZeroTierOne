use crate::pubsub::change_listener::ChangeListener;
use crate::pubsub::protobuf::pbmessages::MemberChange;
use prost::Message;
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::mpsc::Receiver;
use tokio::sync::Mutex;

/**
 * Member Listener listens for member changes and passes them back to the controller
 *
 * This is a wrapper around ChangeListener that specifically handles member changes.
 * It uses a Tokio channel to receive messages and decodes them into MemberChange messages.
 */
pub struct MemberListener {
    change_listener: ChangeListener,
    rx_channel: Mutex<Receiver<Vec<u8>>>,
}

impl MemberListener {
    pub async fn new(controller_id: &str, listen_timeout: Duration) -> Result<Arc<Self>, Box<dyn std::error::Error>> {
        let (tx, rx) = tokio::sync::mpsc::channel(64);

        let change_listener = ChangeListener::new(
            controller_id,
            "controller-member-change-stream",
            format!("{}-member-change-subscription", controller_id).as_str(),
            listen_timeout,
            tx,
        )
        .await?;

        Ok(Arc::new(Self { change_listener, rx_channel: Mutex::new(rx) }))
    }

    pub async fn listen(&self) -> Result<(), Box<dyn std::error::Error>> {
        self.change_listener.listen().await
    }

    pub fn change_handler(self: Arc<Self>) -> Result<(), Box<dyn std::error::Error>> {
        let this = self.clone();
        tokio::spawn(async move {
            let mut rx = this.rx_channel.lock().await;
            while let Some(change) = rx.recv().await {
                if let Ok(m) = MemberChange::decode(change.as_slice()) {
                    print!("Received change: {:?}", m);
                }
            }
        });

        Ok(())
    }
}
