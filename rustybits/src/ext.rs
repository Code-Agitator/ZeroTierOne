/*
 * Copyright (c)2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
#[cfg(feature = "ztcontroller")]
use std::os::raw::c_void;
#[cfg(feature = "ztcontroller")]
use std::sync::Arc;
#[cfg(feature = "ztcontroller")]
use std::time::Duration;
#[cfg(feature = "ztcontroller")]
use tokio::runtime;
use url::Url;

#[cfg(feature = "ztcontroller")]
static mut RT: Option<tokio::runtime::Runtime> = None;

#[cfg(feature = "ztcontroller")]
static START: std::sync::Once = std::sync::Once::new();
#[cfg(feature = "ztcontroller")]
static SHUTDOWN: std::sync::Once = std::sync::Once::new();

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn init_async_runtime() {
    START.call_once(|| {
        let rt = tokio::runtime::Builder::new_multi_thread()
            .worker_threads(4)
            .thread_name("rust-async-worker")
            .enable_all()
            .build()
            .expect("Failed to create tokio runtime");

        unsafe { RT = Some(rt) };
    });
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
#[allow(static_mut_refs)]
pub unsafe extern "C" fn shutdown_async_runtime() {
    SHUTDOWN.call_once(|| {
        // Shutdown the tokio runtime
        unsafe {
            if let Some(rt) = RT.take() {
                rt.shutdown_timeout(std::time::Duration::from_secs(5));
            }
        }
    });
}

#[cfg(feature = "zeroidc")]
use crate::zeroidc::ZeroIDC;

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_new(
    issuer: *const c_char,
    client_id: *const c_char,
    auth_endpoint: *const c_char,
    provider: *const c_char,
    web_listen_port: u16,
) -> *mut ZeroIDC {
    if issuer.is_null() {
        println!("issuer is null");
        return std::ptr::null_mut();
    }

    if client_id.is_null() {
        println!("client_id is null");
        return std::ptr::null_mut();
    }

    if provider.is_null() {
        println!("provider is null");
        return std::ptr::null_mut();
    }

    if auth_endpoint.is_null() {
        println!("auth_endpoint is null");
        return std::ptr::null_mut();
    }

    let issuer = unsafe { CStr::from_ptr(issuer) };
    let client_id = unsafe { CStr::from_ptr(client_id) };
    let provider = unsafe { CStr::from_ptr(provider) };
    let auth_endpoint = unsafe { CStr::from_ptr(auth_endpoint) };
    match ZeroIDC::new(
        issuer.to_str().unwrap(),
        client_id.to_str().unwrap(),
        provider.to_str().unwrap(),
        auth_endpoint.to_str().unwrap(),
        web_listen_port,
    ) {
        Ok(idc) => Box::into_raw(Box::new(idc)),
        Err(s) => {
            println!("Error creating ZeroIDC instance: {}", s);
            std::ptr::null_mut()
        }
    }
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_delete(ptr: *mut ZeroIDC) {
    if ptr.is_null() {
        return;
    }
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.stop();

    unsafe {
        let _ = Box::from_raw(ptr);
    }
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_start(ptr: *mut ZeroIDC) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.start();
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_stop(ptr: *mut ZeroIDC) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.stop();
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_is_running(ptr: *mut ZeroIDC) -> bool {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    idc.is_running()
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_get_exp_time(ptr: *mut ZeroIDC) -> u64 {
    let id = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    id.get_exp_time()
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_set_nonce_and_csrf(
    ptr: *mut ZeroIDC,
    csrf_token: *const c_char,
    nonce: *const c_char,
) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };

    if csrf_token.is_null() {
        println!("csrf_token is null");
        return;
    }

    if nonce.is_null() {
        println!("nonce is null");
        return;
    }

    let csrf_token = unsafe { CStr::from_ptr(csrf_token) }.to_str().unwrap().to_string();
    let nonce = unsafe { CStr::from_ptr(nonce) }.to_str().unwrap().to_string();

    idc.set_nonce_and_csrf(csrf_token, nonce);
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn free_cstr(s: *mut c_char) {
    if s.is_null() {
        println!("passed a null object");
        return;
    }

    unsafe {
        let _ = CString::from_raw(s);
    }
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_get_auth_url(ptr: *mut ZeroIDC) -> *mut c_char {
    if ptr.is_null() {
        println!("passed a null object");
        return std::ptr::null_mut();
    }
    let idc = unsafe { &mut *ptr };

    let s = CString::new(idc.auth_url()).unwrap();
    s.into_raw()
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_token_exchange(idc: *mut ZeroIDC, code: *const c_char) -> *mut c_char {
    if idc.is_null() {
        println!("idc is null");
        return std::ptr::null_mut();
    }

    if code.is_null() {
        println!("code is null");
        return std::ptr::null_mut();
    }
    let idc = unsafe { &mut *idc };

    let code = unsafe { CStr::from_ptr(code) }.to_str().unwrap();

    let ret = idc.do_token_exchange(code);
    match ret {
        Ok(ret) => {
            #[cfg(debug_assertions)]
            {
                println!("do_token_exchange ret: {}", ret);
            }
            let ret = CString::new(ret).unwrap();
            ret.into_raw()
        }
        Err(e) => {
            #[cfg(debug_assertions)]
            {
                println!("do_token_exchange err: {}", e);
            }
            let errstr = format!("{{\"errorMessage\": \"{}\"}}", e);
            let ret = CString::new(errstr).unwrap();
            ret.into_raw()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn zeroidc_get_url_param_value(param: *const c_char, path: *const c_char) -> *mut c_char {
    if param.is_null() {
        println!("param is null");
        return std::ptr::null_mut();
    }
    if path.is_null() {
        println!("path is null");
        return std::ptr::null_mut();
    }
    let param = unsafe { CStr::from_ptr(param) }.to_str().unwrap();
    let path = unsafe { CStr::from_ptr(path) }.to_str().unwrap();

    let url = "http://localhost:9993".to_string() + path;
    let url = Url::parse(&url).unwrap();

    let pairs = url.query_pairs();
    for p in pairs {
        if p.0 == param {
            let s = CString::new(p.1.into_owned()).unwrap();
            return s.into_raw();
        }
    }

    std::ptr::null_mut()
}

#[no_mangle]
pub unsafe extern "C" fn zeroidc_network_id_from_state(state: *const c_char) -> *mut c_char {
    if state.is_null() {
        println!("state is null");
        return std::ptr::null_mut();
    }

    let state = unsafe { CStr::from_ptr(state) }.to_str().unwrap();

    let split = state.split('_');
    let split = split.collect::<Vec<&str>>();
    if split.len() != 2 {
        return std::ptr::null_mut();
    }

    let s = CString::new(split[1]).unwrap();
    s.into_raw()
}

#[cfg(all(
    feature = "zeroidc",
    any(
        all(target_os = "linux", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "linux", target_arch = "aarch64"),
        target_os = "windows",
        target_os = "macos",
    )
))]
#[no_mangle]
pub unsafe extern "C" fn zeroidc_kick_refresh_thread(idc: *mut ZeroIDC) {
    if idc.is_null() {
        println!("idc is null");
        return;
    }
    let idc = unsafe { &mut *idc };

    idc.kick_refresh_thread();
}

#[cfg(feature = "ztcontroller")]
use crate::smeeclient::NetworkJoinedParams;
#[cfg(feature = "ztcontroller")]
use crate::smeeclient::SmeeClient;

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn smee_client_new(
    temporal_url: *const c_char,
    namespace: *const c_char,
    task_queue: *const c_char,
) -> *mut SmeeClient {
    let url = unsafe {
        assert!(!temporal_url.is_null());
        CStr::from_ptr(temporal_url).to_str().unwrap()
    };

    let ns = unsafe {
        assert!(!namespace.is_null());
        CStr::from_ptr(namespace).to_str().unwrap()
    };

    let tq = unsafe {
        assert!(!task_queue.is_null());
        CStr::from_ptr(task_queue).to_str().unwrap()
    };

    match SmeeClient::new(url, ns, tq) {
        Ok(c) => Box::into_raw(Box::new(c)),
        Err(e) => {
            println!("error creating smee client instance: {}", e);
            std::ptr::null_mut()
        }
    }
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn smee_client_delete(ptr: *mut SmeeClient) {
    if ptr.is_null() {
        return;
    }
    let smee = unsafe {
        assert!(!ptr.is_null());
        Box::from_raw(&mut *ptr)
    };
    drop(smee);
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn smee_client_notify_network_joined(
    smee_instance: *mut SmeeClient,
    network_id: *const c_char,
    member_id: *const c_char,
) -> bool {
    let nwid = unsafe {
        assert!(!network_id.is_null());
        CStr::from_ptr(network_id).to_str().unwrap()
    };

    let mem_id = unsafe {
        assert!(!member_id.is_null());
        CStr::from_ptr(member_id).to_str().unwrap()
    };

    let smee = unsafe {
        assert!(!smee_instance.is_null());
        &mut *smee_instance
    };

    let params = NetworkJoinedParams::new(nwid, mem_id);

    match smee.notify_network_joined(params) {
        Ok(()) => true,
        Err(e) => {
            println!("error notifying network joined: {0}", e);
            false
        }
    }
}

#[cfg(feature = "ztcontroller")]
use crate::pubsub::member_listener::MemberListener;
#[cfg(feature = "ztcontroller")]
use crate::pubsub::network_listener::NetworkListener;

#[cfg(feature = "ztcontroller")]
use crate::pubsub::member_listener::MemberListenerCallback;
#[cfg(feature = "ztcontroller")]
use crate::pubsub::network_listener::NetworkListenerCallback;

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn network_listener_new(
    controller_id: *const c_char,
    listen_timeout: u64,
    callback: NetworkListenerCallback,
    user_ptr: *mut c_void,
) -> *const NetworkListener {
    if listen_timeout == 0 {
        println!("listen_timeout is zero");
        return std::ptr::null_mut();
    }
    if controller_id.is_null() {
        println!("controller_id is null");
        return std::ptr::null_mut();
    }

    let id = unsafe { CStr::from_ptr(controller_id) }.to_str().unwrap();

    let rt = runtime::Handle::current();
    rt.block_on(async {
        match NetworkListener::new(id, Duration::from_secs(listen_timeout), callback, user_ptr).await {
            Ok(listener) => Arc::into_raw(listener),
            Err(e) => {
                println!("error creating network listener: {}", e);
                std::ptr::null_mut()
            }
        }
    })
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn network_listener_delete(ptr: *const NetworkListener) {
    if ptr.is_null() {
        return;
    }
    drop(Arc::from_raw(ptr));
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn network_listener_listen(ptr: *const NetworkListener) -> bool {
    use std::mem::ManuallyDrop;
    if ptr.is_null() {
        println!("ptr is null");
        return false;
    }

    let listener = ManuallyDrop::new(unsafe { Arc::from_raw(ptr) });

    let rt = runtime::Handle::current();
    match rt.block_on(listener.listen()) {
        Ok(_) => {
            println!("Network listener started successfully");
            true
        }
        Err(e) => {
            println!("Error starting network listener: {}", e);
            false
        }
    }
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn network_listener_change_handler(ptr: *const NetworkListener) {
    use std::mem::ManuallyDrop;
    if ptr.is_null() {
        println!("ptr is null");
        return;
    }

    let listener = ManuallyDrop::new(unsafe { Arc::from_raw(ptr) });

    let rt = runtime::Handle::current();
    match rt.block_on(listener.change_handler()) {
        Ok(_) => {
            println!("Network listener change listener completed successfully");
        }
        Err(e) => {
            println!("Error in network listener change listener: {}", e);
        }
    }
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn member_listener_new(
    controller_id: *const c_char,
    listen_timeout: u64,
    callback: MemberListenerCallback,
    user_ptr: *mut c_void,
) -> *const MemberListener {
    if listen_timeout == 0 {
        println!("listen_timeout is zero");
        return std::ptr::null_mut();
    }
    if controller_id.is_null() {
        println!("controller_id is null");
        return std::ptr::null_mut();
    }

    let id = unsafe { CStr::from_ptr(controller_id) }.to_str().unwrap();

    let rt = runtime::Handle::current();
    rt.block_on(async {
        match MemberListener::new(id, Duration::from_secs(listen_timeout), callback, user_ptr).await {
            Ok(listener) => Arc::into_raw(listener),
            Err(e) => {
                println!("error creating member listener: {}", e);
                std::ptr::null_mut()
            }
        }
    })
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn member_listener_delete(ptr: *const MemberListener) {
    if ptr.is_null() {
        return;
    }
    drop(Arc::from_raw(ptr));
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn member_listener_listen(ptr: *const MemberListener) -> bool {
    use std::mem::ManuallyDrop;
    if ptr.is_null() {
        println!("ptr is null");
        return false;
    }

    let listener = ManuallyDrop::new(unsafe { Arc::from_raw(ptr) });
    let rt = runtime::Handle::current();
    match rt.block_on(listener.listen()) {
        Ok(_) => {
            println!("Member listener started successfully");
            true
        }
        Err(e) => {
            println!("Error starting member listener: {}", e);
            false
        }
    }
}

#[cfg(feature = "ztcontroller")]
#[no_mangle]
pub unsafe extern "C" fn member_listener_change_handler(ptr: *const MemberListener) {
    use std::mem::ManuallyDrop;
    if ptr.is_null() {
        println!("ptr is null");
        return;
    }

    let listener = ManuallyDrop::new(unsafe { Arc::from_raw(ptr) });

    let rt = runtime::Handle::current();
    match rt.block_on(listener.change_handler()) {
        Ok(_) => {
            println!("Member listener change listener completed successfully");
        }
        Err(e) => {
            println!("Error in member listener change listener: {}", e);
        }
    }
}
