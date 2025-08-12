use gcloud_pubsub::client::{Client, ClientConfig};
use gcloud_pubsub::subscription::SubscriptionConfig;
use gcloud_pubsub::topic::Topic;
use std::time::Duration;
use tokio::sync::mpsc::Sender;
use tokio_util::sync::CancellationToken;

pub struct ChangeListener {
    client: Client,
    topic: Topic,
    subscription_name: String,
    controller_id: String,
    listen_timeout: Duration,
    sender: Sender<Vec<u8>>,
}

impl ChangeListener {
    pub async fn new(
        controller_id: &str,
        topic_name: &str,
        subscription_name: &str,
        listen_timeout: Duration,
        sender: Sender<Vec<u8>>,
    ) -> Result<Self, Box<dyn std::error::Error>> {
        let config = ClientConfig::default().with_auth().await.unwrap();
        let client = Client::new(config).await?;
        let topic = client.topic(topic_name);
        Ok(Self {
            client,
            topic,
            subscription_name: subscription_name.to_string(),
            controller_id: controller_id.to_string(),
            listen_timeout,
            sender,
        })
    }

    pub async fn listen(&self) -> Result<(), Box<dyn std::error::Error>> {
        let config = SubscriptionConfig {
            enable_message_ordering: true,
            filter: format!("attributes.controller_id = '{}'", self.controller_id),
            ..Default::default()
        };

        let subscription = self.client.subscription(self.subscription_name.as_str());
        if !subscription.exists(None).await? {
            subscription
                .create(self.topic.fully_qualified_name(), config, None)
                .await?;
        }

        let cancel = CancellationToken::new();
        let cancel2 = cancel.clone();

        let listen_timeout = self.listen_timeout.clone();
        tokio::spawn(async move {
            tokio::time::sleep(listen_timeout).await;
            cancel2.cancel();
        });

        let tx = self.sender.clone();
        let _ = subscription
            .receive(
                move |message, _cancel| {
                    let tx2 = tx.clone();
                    async move {
                        let data = message.message.data.clone();

                        match tx2.send(data.to_vec()).await {
                            Ok(_) => println!("Message sent successfully"),
                            Err(e) => eprintln!("Failed to send message: {}", e),
                        }

                        match message.ack().await {
                            Ok(_) => println!("Message acknowledged"),
                            Err(e) => eprintln!("Failed to acknowledge message: {}", e),
                        }
                    }
                },
                cancel.clone(),
                None,
            )
            .await;

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    // use super::*;

    // use testcontainers::runners::AsyncRunner;
    // use testcontainers_modules::google_cloud_sdk_emulators;
    // use tokio;

    // #[tokio::test(flavor = "multi_thread", worker_threads = 1)]
    // async fn setup_pubsub_emulator() -> Result<(), Box<dyn std::error::Error + 'static>> {
    //     let container = google_cloud_sdk_emulators::CloudSdk::pubsub().start().await?;

    //     let port = container.get_host_port_ipv4(8085).await?;
    //     let host = format!("localhost:{}", port);

    //     unsafe {
    //         std::env::set_var("PUBSUB_EMULATOR_HOST", host);
    //     }

    //     let cl = ChangeListener::new(
    //         "test_controller",
    //         "test_topic",
    //         "test_subscription",
    //         Duration::from_secs(10),
    //     )
    //     .await;

    //     assert!(cl.is_ok(), "Failed to connect to pubsub emulator: {:?}", cl.err());

    //     Ok(())
    // }
}
