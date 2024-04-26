#![cfg_attr(
    all(not(debug_assertions), target_os = "windows"),
    windows_subsystem = "windows"
)]

mod serial;
use anyhow::{anyhow, bail};
use core::time;
use futures::future::join_all;
use serde::{Deserialize, Serialize};
use serial::get_port_instance;
use std::error::Error;
use std::ops::Deref;
use std::sync::{atomic::AtomicBool, Arc, Mutex};
use std::thread::{self, JoinHandle};
use tauri::async_runtime::spawn_blocking;
use tauri::{
    api::process::{Command, CommandEvent},
    async_runtime::{channel, Receiver, Sender},
    App, AppHandle, Config, Manager, Runtime, State,
};
use tokio::sync::{oneshot, RwLock};
use tokio::time::timeout;

// Need to implement serde::deserialize trait on this to use in command
#[derive(Deserialize, Serialize, Debug)]
struct NewCard {
    name: String,
    password: String,
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "request_type")]
enum DongleRequest {
    // TX
    // https://serde.rs/variant-attrs.html
    #[serde(rename = "get_pass_descs")]
    GetPasswordDescriptionsAndSwitchReader,
    BoardSwitchMain,

    #[serde(rename = "send_new_card")]
    NewCard(NewCard),
    ClearPasswords,
}

impl DongleRequest {
    fn has_response(&self) -> bool {
        match self {
            DongleRequest::GetPasswordDescriptionsAndSwitchReader => true,
            DongleRequest::NewCard(_) => true,
            _ => false,
        }
    }
    
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "response_type")]
enum DongleResponse {
    // RX
    #[serde(rename = "get_pass_descs")]
    SendPasswordDescriptions(PasswordDescriptions),

    #[serde(rename = "detected_rfid")]
    RFIDDetected(RFID),

    #[serde(rename = "send_new_card")]
    ConfirmNewCard
}

#[derive(Serialize, Deserialize, Debug)]
struct PasswordLessCard {
    name: String,
    rfid: u64,
}

#[derive(Serialize, Deserialize, Debug)]
struct PasswordDescriptions {
    descriptions: Vec<PasswordLessCard>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
struct RFID {
    rfid: u64,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
#[serde(tag = "type")]
enum KeyDotErrors {
    CouldNotOpenPort { error: String },
    NoPortConnection { error: String },
    CouldNotStopSerialThread { error: String },
    UndigestableJson { error: String, json: String },
    ServerError { error: String },
}

type ReaderThreadLock = Arc<RwLock<Option<SerialThreadInstance>>>;

// You can have multiple states in tauri app, check https://github.com/tauri-apps/tauri/blob/dev/examples/state/main.rs
//State is just a wrapper on Arc, so you need to use Mutexes ursefl, it allows this to imeplement DerefMut
struct ReaderThreadState {
    // Rwlock (tokio) or Mutex(std) can be used here since they implement Send+Sync
    reader_thread: ReaderThreadLock,
    // Sending this to another thread hence the Arc
    kill_signal: Arc<AtomicBool>,
}
#[derive(Debug)]
pub struct SerialThreadRequest {
    payload: DongleRequest,
    ret: oneshot::Sender<anyhow::Result<DongleResponse>>,
}

// Dont need to store a port instance on this since the PORT object is closed when out of scope
struct SerialThreadInstance {
    thread: thread::JoinHandle<anyhow::Result<()>>,
    sender: Sender<SerialThreadRequest>,
}

#[derive(Default)]
struct MyState {
    s: std::sync::Mutex<String>,
    t: std::sync::Mutex<std::collections::HashMap<String, String>>,
}
// remember to call `.manage(MyState::default())`
#[tauri::command]
async fn command_name(state: tauri::State<'_, MyState>) -> Result<(), String> {
    *state.s.lock().unwrap() = "new string".into();
    state.t.lock().unwrap().insert("key".into(), "value".into());
    Ok(())
}

#[tauri::command]
async fn test<R: Runtime>(
    app: tauri::AppHandle<R>,
    window: tauri::Window<R>,
) -> Result<String, String> {
    // `new_sidecar()` expects just the filename, NOT the whole path like in JavaScript
    // 'bin/dist/parttool', [`-e`, `${espBin}`, `--port`, `COM4`, `--baud`, `115200`, `read_partition`, `--partition-name=nvs`,`--output`, binFileName
    let mut esp_bin = std::env::current_exe()
        .unwrap()
        .parent()
        .unwrap()
        .to_owned();
    esp_bin.push("esptool");
    let esp_bin = esp_bin.as_os_str().to_str().unwrap();
    let mut command_builder = Command::new_sidecar("parttool").unwrap().args([
        "-e",
        &esp_bin,
        "--port",
        "COM4",
        "--baud",
        "115200",
        "read_partition",
        "--partition-name=nvs",
        "--output",
        "./Hey_man.bin",
    ]);
    println!("Running command{:?}", command_builder);

    let (mut rx, mut child) = command_builder.spawn().expect("Failed to spawn sidecar");

    // THis is effectively the same thing as just doing it through js, theres no api yet for reading bytes from stdin
    tauri::async_runtime::spawn(async move {
        println!("Reading events from command");
        // read events such as stdout
        while let Some(event) = rx.recv().await {
            if let CommandEvent::Stdout(line) = event {
                // window
                //     .emit("message", Some(format!("'{}'", line)))
                //     .expect("failed to emit event");
                println!("{line}");

                // write to stdin
                // child.write("message from Rust\n".as_bytes()).unwrap();
            } else if let CommandEvent::Stderr(line) = event {
                println!("{line}");
            }
        }
    });
    // let printThing = format!("test: {:#?}", app.config());
    // println!("{printThing}");
    // Ok(printThing)
    Ok((String::new()))
}


#[tauri::command]
async fn get_current_working_dir() -> Result<String, String> {
    match std::env::current_exe() {
        Ok(exe_path) => match exe_path.parent() {
            Some(parent_dir) => match parent_dir.to_str() {
                Some(parent_dir_str) => Ok(parent_dir_str.to_owned()),
                None => Err("Could not convert parent directory path to string".to_owned()),
            },
            None => Err("Could not get parent directory of current executable".to_owned()),
        },
        Err(e) => Err(format!("Could not get path of current executable: {}", e)),
    }
}

fn kill_old_server_thread(
    maybe_old_thread: &mut Option<SerialThreadInstance>,
    // Techinically the atomic bool here is mutable because the reference to the arc is just a reference to heap
    killer: Arc<AtomicBool>,
) -> anyhow::Result<bool> {
    // Take grabs the value from Option, leaving a none in its place, we need the value in order to actually run join
    match maybe_old_thread.take() {
        Some(old_thread) => {
            println!("Stopping old thread");
            killer.store(true, std::sync::atomic::Ordering::SeqCst);
            old_thread
                .thread
                .join()
                .map_err(|e| anyhow!("Could not join old thread"))?
                // // The map here propogates the result string if its err, but exports an Ok(true) if not
                .map(|_| true)
        }
        None => Ok(false),
    }
}

#[tauri::command]
async fn start_listen_server(
    window: tauri::Window,
    // Anonymous lifetime specified here because the state object needs to exist for the duration of the function and has reference to AppState
    mut state: State<'_, ReaderThreadState>,
    port: String,
) -> Result<(), KeyDotErrors> {
    println!("Killing old thread if it exists");
    let mut maybe_old_thread = state.reader_thread.write().await;

    let old_kill_res = kill_old_server_thread(&mut maybe_old_thread, state.kill_signal.clone());
    println!("{:?}", old_kill_res);

    // Using release here to ensure syncronisation of the CPU cache on store so that it gets commited to all threads,
    // Release is only paired with store and doing this is better than just using SeqCst (Sequentially consistent) everywhere
    state
        .kill_signal
        .store(false, std::sync::atomic::Ordering::SeqCst);
    println!("Starting reader");

    // let port = get_port_instance(&port).map_err(|e| KeyDotErrors::CouldNotOpenPort(e.to_string()))?;
    let port = get_port_instance(&port).unwrap();

    let app = window.app_handle().clone();
    let kil_signal_clone = state.kill_signal.clone();
    let (sender, receiver) = channel(32);

    let rw_lock_clone = state.reader_thread.clone();
    let killer_clone = state.kill_signal.clone();

    let kill_callback = move || {
        // Ok so this blocking call should be fine since the kill callback is likely to return anyway, FnOnce guarantees that too
        let mut maybe_old_thread = rw_lock_clone.blocking_write();
        // Cant join from the thread itself, dont need to call kill_old_thread, besides that makes no sense
        *maybe_old_thread = None;
        anyhow::Ok(())
    };

    let thread_handle = thread::spawn(move || {
        serial::serial_comms_loop(app, kil_signal_clone, port, receiver, kill_callback)
    });

    *maybe_old_thread = Some(SerialThreadInstance {
        thread: thread_handle,
        sender,
    });

    // This returns the error if it exists after the new one got started
    // old_kill_res?;

    Ok(())
}

#[tauri::command]
async fn test_sync_loop(mut state: tauri::State<'_, ReaderThreadState>) -> Result<String, String> {
    // Assume listen server is on
    #[cfg(feature = "test-sync")]
    {
        // Make multiple concurrent requests to get_cards_db
        let mut handles = vec![];
        for _ in 0..10 {
            let state_clone = state.reader_thread.clone();
            let handle = tauri::async_runtime::spawn(async move {
                let cards = test_send_queries(state_clone).await;
                println!("{:?}", cards);
            });
            handles.push(handle);
        }
        // Wait for all the handles to finish
        join_all(handles).await;

        // Other thread should try and kill itself
    }

    Ok("Hey".to_owned())
}

#[cfg(feature = "test-sync")]
async fn test_send_queries(reader_thread: ReaderThreadLock) {
    let state = reader_thread.read().await;
    if let Some(state) = &*state {
        let (ret_tx, ret_rx) = oneshot::channel();
        let request = SerialThreadRequest {
            payload: DongleRequest::GetPasswordDescriptionsAndSwitchReader,
            ret: ret_tx,
        };
        let serial_sender = &state.sender;
        serial_sender.send(request).await.unwrap();
    }
}

#[tauri::command]
async fn get_cards_db(
    state: tauri::State<'_, ReaderThreadState>,
) -> Result<PasswordDescriptions, String> {
    println!("Getting cards");
    let mut maybe_sender = None;
    {
        let state = state.reader_thread.read().await;
        if let Some(state) = &*state {
            let serial_sender = state.sender.clone();
            maybe_sender = Some(serial_sender);
        }
    }
    if let Some(sender) = maybe_sender {
        let (ret_tx, ret_rx) = oneshot::channel();
        let request = SerialThreadRequest {
            payload: DongleRequest::GetPasswordDescriptionsAndSwitchReader,
            ret: ret_tx,
        };
        sender.send(request).await.unwrap();
        println!("Sent req to reader_thread");

        let response = ret_rx.await.unwrap();
        let response = response.map_err(|e| e.to_string())?;
        if let DongleResponse::SendPasswordDescriptions(password_descs) = response {
            Ok(password_descs)
        } else {
            Err("Did not get password payload".to_owned())
        }
    } else {
        Err("Could not get sender".to_owned())
    }
}

#[tauri::command]
async fn send_new_card(card: NewCard, state: tauri::State<'_, ReaderThreadState>) -> Result<(), String> {
    println!("Sending card {:?}", card);
    return Ok(());
    let mut maybe_sender = None;
    {
        let state = state.reader_thread.read().await;
        if let Some(state) = &*state {
            let serial_sender = state.sender.clone();
            maybe_sender = Some(serial_sender);
        }
    }

     if let Some(sender) = maybe_sender {
        let (ret_tx, ret_rx) = oneshot::channel();
        let request = SerialThreadRequest {
            payload: DongleRequest::NewCard(card),
            ret: ret_tx,
        };
        sender.send(request).await.unwrap();
        println!("Sent req to reader_thread");

        let response = ret_rx.await.unwrap();
        let response = response.map_err(|e| e.to_string())?;
        if let DongleResponse::ConfirmNewCard = response {
            Ok(())
        } else {
            Err("Did not get password payload".to_owned())
        }
    } else {
        Err("Could not get sender".to_owned())
    }
}

// This HAS to be async in order to join properly so that the join doesnt block the main thread
#[tauri::command]
async fn stop_listen_server(state: State<'_, ReaderThreadState>) -> Result<bool, KeyDotErrors> {
    let mut maybe_old_thread = state.reader_thread.write().await;
    kill_old_server_thread(&mut maybe_old_thread, state.kill_signal.clone()).map_err(|e| {
        KeyDotErrors::CouldNotStopSerialThread {
            error: e.to_string(),
        }
    })
}

#[tauri::command]
fn get_ports() -> Vec<String> {
    let ports = serialport::available_ports().expect("No ports found!");
    ports.iter().map(|p| p.port_name.clone()).collect()
    // for p in ports {
    //     println!("{}", p.port_name);
    // }
}

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            get_ports,
            start_listen_server,
            test,
            get_current_working_dir,
            stop_listen_server,
            get_cards_db,
            send_new_card,
            test_sync_loop
        ])
        // .setup(|app| setup(app))
        .manage(ReaderThreadState {
            reader_thread: Arc::new(RwLock::new(None)),
            kill_signal: Arc::new(AtomicBool::new(true)),
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
// Return type says the error can basically fill any type
fn setup(app: &App) -> Result<(), Box<(dyn std::error::Error)>> {
    // Not entirely sure, but perhaps you could omit that error type
    let app_handle = app.handle();

    thread::spawn(move || {
        // serial::read_rfid(app_handle);
        test_loop(app_handle);
    });
    Ok(())
}

fn test_loop(app: AppHandle) {
    let millis_100 = time::Duration::from_millis(100);
    let mut counter = 0;
    loop {
        thread::sleep(millis_100);
        let payload = format!("Hey man {}", counter);
        // app.emit_all("rfid", payload);
        counter += 1;
    }
}
