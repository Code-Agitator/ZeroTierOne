extern crate cbindgen;

use cbindgen::{Config, Language, MacroExpansionConfig};
use std::env;
use std::path::PathBuf;
fn main() {
    #[cfg(feature = "ztcontroller")]
    {
        let mut prost_build = prost_build::Config::new();

        prost_build
            .type_attribute(".", "#[derive(serde::Serialize, serde::Deserialize)]")
            .compile_protos(
                &[
                    "src/pubsub/network.proto",
                    "src/pubsub/member.proto",
                    "src/pubsub/member_status.proto",
                ],
                &["src/pubsub/"],
            )
            .expect("Failed to compile protobuf files");
    }

    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

    let package_name = env::var("CARGO_PKG_NAME").unwrap();
    let output_file = target_dir().join(format!("{package_name}.h")).display().to_string();

    let meconfig = MacroExpansionConfig { bitflags: true, ..Default::default() };

    let config = Config {
        language: Language::C,
        cpp_compat: true,
        namespace: Some(String::from("rustybits")),
        macro_expansion: meconfig,
        ..Default::default()
    };

    cbindgen::generate_with_config(&crate_dir, config)
        .unwrap()
        .write_to_file(&output_file);
}

/// Find the location of the `target/` directory. Note that this may be
/// overridden by `cmake`, so we also need to check the `CARGO_TARGET_DIR`
/// variable.
fn target_dir() -> PathBuf {
    if let Ok(target) = env::var("CARGO_TARGET_DIR") {
        PathBuf::from(target)
    } else {
        PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("target")
    }
}
