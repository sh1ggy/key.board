#![cfg_attr(
    all(not(debug_assertions), target_os = "windows"),
    windows_subsystem = "windows"
)]

mod serial;
use anyhow::{anyhow, bail};
use serde::{Deserialize, Serialize};
use serial::get_port_instance;
use std::borrow::Borrow;
use std::thread::{self, JoinHandle};
use std::sync::mpsc::Sender;
use std::{
    error::Error,
    sync::{atomic::AtomicBool, Arc, Mutex},
    time::{self},
};
use tauri::{
    api::process::{Command, CommandEvent},
    App, AppHandle, Config, Manager, Runtime, State,
};

// Need to implement serde::deserialize trait on this to use in command
#[derive(Deserialize, Serialize, Debug)]
struct Card {
    name: String,
    password: String,
    rfid: String,
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "request_type")]
enum DongleRequest {
    // TX
    BoardSwitchRead,
    GetPasswordDescriptions,
    BoardSwitchMain,
    NewCard(Card),
    ClearPasswords,

    // RX
    SendPasswordDescriptions(PasswordDescriptions),
    RFIDDetected(RFID),
}

#[derive(Serialize, Deserialize, Debug)]
struct PasswordLessCard {
    description: String,
    password: String,
}

#[derive(Serialize, Deserialize, Debug)]
struct PasswordDescriptions {
    descriptions: Vec<PasswordLessCard>,
}

#[derive(Serialize, Deserialize, Debug)]
struct RFID {
    rfid: String,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
#[serde(tag = "type")]
enum KeyDotErrors {
    CouldNotOpenPort { error: String },
    CouldNotReadPort,
    CouldNotWritePort,
    CouldNotStopSerialThread { error: String },
    CouldNotReadPortBytes,
    UndigestableJson { error: String, json: String },
}

// You can have multiple states in tauri app, check https://github.com/tauri-apps/tauri/blob/dev/examples/state/main.rs
struct ReaderThreadState {
    // Mutex allows our struct to implement DerefMut
    reader_thread: Mutex<Option<SerialThreadInstance>>,
    kill_signal: Arc<AtomicBool>,
}

struct SerialThreadInstance {
    thread: thread::JoinHandle<anyhow::Result<()>>,
    sender: Sender<String>,
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

fn save_cards_to_csv(cards: Vec<Card>, config: Arc<Config>) -> Result<String, Box<dyn Error>> {
    // TODO: Use anyhow to propogate errors in this in a way that doesnt need to make a new function or use a closure
    let mut path =
        tauri::api::path::app_local_data_dir(&config).unwrap_or(std::path::PathBuf::from("./temp"));
    std::fs::create_dir_all(&path)?;

    path.push("to_save.csv");
    println!("Saving csv at: {:?}", path);
    let mut wtr = csv::Writer::from_path(&path)?;
    wtr.write_record(&["key", "type", "encoding", "value"])?;
    wtr.write_record(&["kb", "namespace", "", ""])?;

    let mut uid_buffer = String::new();
    let uid_count = cards.len();

    for (i, card) in cards.iter().enumerate() {
        // println!("card lol: {:?}", card);

        // let mut my_vector: Vec<&str> = Vec::new();

        // my_vector.push(&card.name);

        let key = format!("name{}", i.to_string());
        let card_name = &card.name;
        let record = [&key, "data", "string", card_name];
        wtr.write_record(record)?;

        let key = format!("pass{}", i.to_string());
        let card_pass = &card.password;
        let record = [&key, "data", "string", card_pass];
        wtr.write_record(record)?;

        // let hex_string_trimmed: String = hex_string
        //     .replace('\0', "")
        //     .trim()
        //     .chars()
        //     .filter(|c| !c.is_whitespace())
        //     .collect();

        // let maybe_hex = hex::decode(hex_string_trimmed);
        let new_uid_string = card.rfid.trim().replace(" ", "");
        uid_buffer.push_str(&new_uid_string);
    }
    wtr.write_record(&["uids", "data", "hex2bin", &uid_buffer])?;
    wtr.write_record(&["num_cards", "data", "u32", &uid_count.to_string()])?;

    match path.to_str() {
        Some(str) => Ok(str.into()),
        None => Err(
            "HEy man, path for path buf could not be computed, prolly not a valid utf-8 string"
                .into(),
        ),
    }
}

// We cant use this because dyn Error doesnt implement Serialize but string does :)
// async fn save_card(value: String) -> Result<(), Box<dyn Error>> {
#[tauri::command]
async fn save_cards_to_csv_command(
    app: AppHandle,
    cards: Vec<Card>,
    // port: String,
) -> Result<String, String> {
    // let confRef = &app.config();

    // Because we are now using the value of path_to_csv, the config reference that has to be passed into save_cards becomes invalid because the return type may use it later
    let path_to_csv = save_cards_to_csv(cards, app.config());

    // Ok(("Hey".into()))
    match path_to_csv {
        Ok(path_to_csv) => Ok(path_to_csv.to_string()),
        Err(err) => {
            let err_string = format!("Could not csv: {}", err.to_string());
            println!("{err_string}");
            return Err(err_string);
        }
    }
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
    killer: &Arc<AtomicBool>,
) -> anyhow::Result<bool> {
    // Take grabs the value from Option, leaving a none in its place, we need the value in order to actually run join
    match maybe_old_thread.take() {
        Some(old_thread) => {
            println!("Stopping old thread");
            killer.store(true, std::sync::atomic::Ordering::SeqCst);
            old_thread
                .thread
                .join()
                .unwrap_or(bail!("Could not join thread"))
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
    state: State<'_, ReaderThreadState>,
    port: String,
) -> Result<(), KeyDotErrors> {
    let mut maybe_old_thread = state.reader_thread.lock().unwrap();

    let old_kill_res = kill_old_server_thread(&mut maybe_old_thread, &state.kill_signal);
    println!("{:?}", old_kill_res);

    // Using release here to ensure syncronisation of the CPU cache on store so that it gets commited to all threads,
    // Release is only paired with store and doing this is better than just using SeqCst (Sequentially consistent) everywhere
    state
        .kill_signal
        .store(false, std::sync::atomic::Ordering::SeqCst);
    println!("Starting reader");

    // let port = get_port_instance(&port).map_err(|e| KeyDotErrors::CouldNotOpenPort(e.to_string()))?;
    let port = get_port_instance(&port).map_err(|e| KeyDotErrors::CouldNotOpenPort {
        error: e.to_string(),
    })?;

    let app = window.app_handle().clone();
    let kil_signal_clone = state.kill_signal.clone();

    let thread_handle =
        thread::spawn(move || serial::serial_comms_loop(app, kil_signal_clone, port));

    *maybe_old_thread = Some(SerialThreadInstance {
        thread: thread_handle,
        sender: 
    });

    // This returns the error if it exists after the new one got started
    // old_kill_res?;

    Ok(())
}

#[tauri::command]
async fn get_cards_db(window: tauri::Window, state: tauri::State<'_, ReaderThreadState>) -> Result<Vec<Card>, String> {
    let handle = tauri::async_runtime::spawn(async {
        // Your long running task here
        // This is just an example, replace it with your actual task
        tokio::time::sleep(std::time::Duration::from_secs(2)).await;
        let result = "Hello, world!".to_string();
        result
    });

    let result = handle.await.map_err(|e| e.to_string())?;
    let app_handle = window.app_handle();

    Ok(vec![])
    // //  Send serial message to get cards
    // tauri::async_runtime::spawn(async move {
    //     // A loop that takes output from the async process and sends it
    //     // to the webview via a Tauri Event
    //     loop {
    //         if let Some(output) = async_proc_output_rx.recv().await {
    //             rs2js(output, &app_handle);
    //         }
    //     }
    // });
}

// This HAS to be async in order to join properly so that the join doesnt block the main thread
#[tauri::command]
async fn stop_listen_server(state: State<'_, ReaderThreadState>) -> Result<bool, KeyDotErrors> {
    let mut maybe_old_thread = state.reader_thread.lock().unwrap();
    kill_old_server_thread(&mut maybe_old_thread, &state.kill_signal).map_err(|e| {
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
            save_cards_to_csv_command,
            get_ports,
            start_listen_server,
            test,
            get_current_working_dir,
            stop_listen_server
        ])
        // .setup(|app| setup(app))
        .manage(ReaderThreadState {
            reader_thread: Mutex::new(None),
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
