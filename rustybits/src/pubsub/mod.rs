/*
 * Copyright (c)2023 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2027-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */

use gcloud_pubsub::client::{Client, ClientConfig};

pub struct PubSubClient {
    client: Client,
}

impl PubSubClient {
    pub async fn new() -> Result<Self, Box<dyn std::error::Error>> {
        let config = ClientConfig::default().with_auth().await.unwrap();
        let client = Client::new(config).await?;

        // Assuming a topic name is required for the client
        let topic_name = "default-topic".to_string();

        Ok(Self { client })
    }
}
