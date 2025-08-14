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
        if !topic.exists(None).await? {
            topic.create(None, None).await?;
        }

        Ok(Self {
            client,
            topic,
            subscription_name: subscription_name.to_string(),
            controller_id: controller_id.to_string(),
            listen_timeout,
            sender,
        })
    }

    /**
     * Listens for changes on the topic and sends them to the provided sender.
     *
     * Listens for up to `listen_timeout` duration, at which point it will stop listening
     * and return.  listen will have to be called again to continue listening.
     *
     * If the subscription does not exist, it will create it with the specified configuration.
     */
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
pub(crate) mod tests {
    use super::*;

    use testcontainers::runners::AsyncRunner;
    use testcontainers::ContainerAsync;
    use testcontainers_modules::google_cloud_sdk_emulators;
    use testcontainers_modules::google_cloud_sdk_emulators::CloudSdk;
    use tokio;

    pub(crate) async fn setup_pubsub_emulator() -> Result<(ContainerAsync<CloudSdk>, String), Box<dyn std::error::Error>>
    {
        let container = google_cloud_sdk_emulators::CloudSdk::pubsub().start().await?;
        let port = container.get_host_port_ipv4(8085).await?;
        let host = format!("localhost:{}", port);

        unsafe {
            std::env::set_var("PUBSUB_EMULATOR_HOST", host.clone());
        }

        Ok((container, host))
    }

    #[tokio::test(flavor = "multi_thread", worker_threads = 1)]
    async fn test_can_connect_to_pubsub() -> Result<(), Box<dyn std::error::Error + 'static>> {
        let (_container, _host) = setup_pubsub_emulator().await?;

        let (tx, _rx) = tokio::sync::mpsc::channel(64);

        let cl = ChangeListener::new(
            "test_controller",
            "test_topic",
            "test_subscription",
            Duration::from_secs(10),
            tx,
        )
        .await;

        assert!(cl.is_ok(), "Failed to connect to pubsub emulator: {:?}", cl.err());

        Ok(())
    }
}
