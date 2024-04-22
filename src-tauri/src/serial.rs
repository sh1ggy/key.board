use anyhow::{anyhow, bail, Result};
use core::time::Duration;
use serde::{Deserialize, Serialize};
use serialport::SerialPort;
use std::{
    io,
    sync::{atomic::AtomicBool, Arc},
    thread,
    time::SystemTime,
};
use tauri::{async_runtime::Receiver, AppHandle, Manager};

use crate::{DongleRequest, DongleResponse, KeyDotErrors, SerialThreadRequest};

pub fn get_port_instance(port_path: &str) -> anyhow::Result<Box<dyn serialport::SerialPort>> {
    const BAUD_RATE: u32 = 115_200;
    let res = serialport::new(port_path, BAUD_RATE)
        .timeout(Duration::from_millis(10))
        .open()?;
    Ok(res)
}

const REQUEST_TIMEOUT: Duration = Duration::from_secs(500);

#[cfg(feature = "test-sync")]
// 5 seconds after starting the thread will die
const TEST_TIMOUT: Duration = Duration::from_secs(5);

pub fn serial_comms_loop(
    app: AppHandle,
    kill_signal: Arc<AtomicBool>,
    mut port: Box<dyn SerialPort>,
    mut request_handler: Receiver<SerialThreadRequest>,
    // Callback to run when the thread dies, may need to be async
    kill_callback: impl FnOnce() -> Result<()>,
) -> anyhow::Result<()> {
    // app.emit_to(label, event, payload)
    println!("Listening on port {:?}", port.name());
    let start = SystemTime::now();

    // thread::sleep(duration::Duration::from_secs(2));
    loop {
        let mut serial_buf: Vec<u8> = Vec::new();
        let mut current_request: Option<(SerialThreadRequest, SystemTime)> = None;
        loop {
            // This loop runs every available frame
            if kill_signal.load(std::sync::atomic::Ordering::SeqCst) {
                println!("Kill signal recieved, killing thread");
                return Ok(());
            }

            #[cfg(feature = "test-sync")]
            if (start.elapsed().unwrap() > TEST_TIMOUT) {
                println!("Test timeout reached, killing thread");
                kill_callback()?;
                return Ok(());
            }

            // Check if we have a request from the main thread
            if let Ok(request) = request_handler.try_recv() {
                // Try and send the request to the dongle, conver the payload into bytes
                // serde_json::to_writer(&mut port, &request.payload).unwrap();
                let send = serde_json::to_string(&request.payload).unwrap();
                println!("Sending to serial {}", send);
                port.write_all(send.as_bytes());

                // If the request doesnt wait for a response, set to None
                if (request.payload.has_response()) {
                    current_request = Some((request, SystemTime::now()));
                }
            }

            // Check the current request isnt stale in timeouts
            if let Some((_, time)) = current_request {
                // dbg!(time.elapsed().unwrap());
                if time.elapsed().unwrap() > REQUEST_TIMEOUT {
                    if let Some((request, _)) = current_request {
                        dbg!(&request);
                        eprint!("Request timed out for {:?}", request.payload);
                        request
                            .ret
                            .send(Err(anyhow!("Request timed out")))
                            .unwrap_or_else(|err| eprintln!("Failed to send error: {:?}", err));

                        current_request = None;
                    }
                }
            }

            // println!("in loop");

            match (port.bytes_to_read()) {
                Ok(bytes_count) => {
                    if bytes_count > 0 {
                        port.read_to_end(&mut serial_buf);
                        break;
                    }
                }
                Err(err) => {
                    println!("Failed to read from port: {}", err);
                    let err = KeyDotErrors::NoPortConnection {
                        error: err.to_string(),
                    };
                    app.emit_all("error", &err)?;
                    kill_callback()?;
                    bail!("Failed to read from port: {:?}", err);
                }
            }
        }

        //Maybe json
        let maybe_dongle_request: Result<DongleResponse, serde_json::Error> =
            serde_json::from_slice(&serial_buf);

        match (maybe_dongle_request) {
            Ok(response) => {
                // let request: DongleRequest = serde_json::from_slice(&serial_buf)?;
                println!("WORKING!!!: {:?}", &response);
                if let Some((request, _)) = current_request {
                    if (request.payload.is_correct_response(&response)) {
                        request
                            .ret
                            .send(Ok(response))
                            .unwrap_or_else(|err| eprintln!("Failed to send error: {:?}", err));
                    } else {
                        request
                            .ret
                            .send(Err(anyhow!("Incorrect response")))
                            .unwrap_or_else(|err| eprintln!("Failed to send error: {:?}", err));
                    }
                } else {
                    app.emit_all("dongle_response", &response)?;
                }
            }
            Err(err) => {
                if let Ok(json_string) = String::from_utf8(serial_buf.clone()) {
                    eprintln!("Unreadable JSON: {}: err:{}", json_string, err);
                    app.emit_all(
                        "error",
                        KeyDotErrors::UndigestableJson {
                            error: err.to_string(),
                            json: json_string,
                        },
                    )?;
                }
                // bail!("Fuck");
            }
        }
        serial_buf.clear();
    }
}

// For now string is fine, otherwise use, Result<(), Box<dyn std::error::Error>> or anyhow OR use this:
// #[derive(Debug)]
// enum ThreadError {
//     FileError(io::Error),
//     PortError(io::Error),
// }
// fn thread_function() -> Result<(), ThreadError> {

pub fn read_rfid_old(
    app: AppHandle,
    kill_signal: Arc<AtomicBool>,
    port_path: String,
) -> Result<(), String> {
    // pub fn read_rfid() {
    // let app = process::app_handle().expect("failed to get app handle");
    // const PORT_PATH: &str = "COM4";
    const BAUD_RATE: u32 = 115_200;
    let port = serialport::new(&port_path, BAUD_RATE)
        .timeout(Duration::from_millis(10))
        .open();
    // On drop of port, it automatically cleans up for you
    // .unwrap();

    match port {
        Ok(mut port) => {
            println!("Receiving data on {} at {} baud:", &port_path, &BAUD_RATE);

            loop {
                let mut serial_buf: Vec<u8> = Vec::new();
                loop {
                    if kill_signal.load(std::sync::atomic::Ordering::SeqCst) {
                        println!("Kill signal recieved, killing thread");
                        return Ok(());
                    }
                    // This is an array of size 1
                    let mut byte = [0; 1];
                    // TODO: dont unwrap here, this likely means the device may have been disconnected
                    if port.bytes_to_read().unwrap() > 0 {
                        let port_val = port.read(&mut byte);
                        match port_val {
                            Ok(_) => {
                                if byte[0] == b'\n' {
                                    break;
                                }
                                if byte[0] != 0 {
                                    serial_buf.push(byte[0]);
                                }
                            }
                            Err(err) => {
                                println!("Failed to read from port{}", err);
                                return Err(err.to_string());
                            }
                        }
                    }
                }

                // The above method reads single byte by byte from serial, possible to also read whole serial buffer and split on newlines
                // This proves the string we get is not immediately equal to the hex equivilant
                // println!("Testing !!!: {}", hex::encode(&serial_buf));
                println!("Testing !!!: {:?}", serial_buf);

                let maybe_hex_string = String::from_utf8(serial_buf.clone());
                if let Ok(hex_string) = maybe_hex_string {
                    println!("Got string {}", hex_string);
                    let hex_string_trimmed: String = hex_string
                        .replace('\0', "")
                        .trim()
                        .chars()
                        .filter(|c| !c.is_whitespace())
                        .collect();
                    let maybe_hex = hex::decode(&hex_string_trimmed);
                    match maybe_hex {
                        Ok(hex_value) => {
                            let back_to_string = hex::encode(&hex_value);
                            let emit_res = app.emit_all("rfid", &back_to_string);
                            if let (Err(emit_err)) = emit_res {
                                eprintln!(
                                    "Could not emit: {} with error: {}",
                                    &back_to_string,
                                    emit_err.to_string()
                                );
                            }
                            println!("WORKING!!!: {}", &back_to_string)
                        }
                        Err(err) => println!("Unreadable Hex {}: {}", &hex_string_trimmed, err),
                    }
                }
            }
        }
        Err(e) => {
            // Emit error to client
            eprintln!("Failed to open \"{}\". Error: {}", port_path, e);
            // Doing this is the same as using the ? error chaining operator on the port value, were just handling the matches ourselves
            Err(e.to_string())
            // ::std::process::exit(1);
        }
    }
}

#[cfg(test)]
mod tests {
    use std::io::repeat;

    use serde::{Deserialize, Serialize};

    use crate::{Card, DongleRequest};

    use super::get_port_instance;

    #[test]
    fn print_env_vars() {
        for (key, value) in std::env::vars() {
            println!("{}: {}", key, value);
        }
    }

    #[test]
    fn test_devices_hid() {
        let api = hidapi::HidApi::new().unwrap();
        // Print out information about all connected devices
        for device in api.device_list() {
            println!("{:#?}", device);
        }
    }

    #[test]
    fn test_read_hid() {
        let api = hidapi::HidApi::new().unwrap();
        let keydot = api.open(12346, 16389).expect("Failed to open device");

        loop {
            let mut buf = [0u8; 256];
            let res = keydot.read(&mut buf[..]).unwrap();

            let mut data_string = String::new();

            for u in &buf[..res] {
                data_string.push_str(&(u.to_string() + "\t"));
            }

            println!("{}", data_string);
        }
    }

    #[derive(Serialize, Deserialize)]
    struct StructA {
        otherprop1: String,
        prop1: u32,
    }

    #[derive(Serialize, Deserialize)]
    #[serde(tag = "type")]
    #[repr(u32)] // Repr here doesnt work, you have to use a conversion crate or manually do like in bebop
    enum MyEnum {
        A(StructA),
        //Following doesnt work
        // B(String),
        C,
    }

    #[test]
    fn test_serde() {
        let a = MyEnum::A(StructA {
            prop1: 32,
            otherprop1: "test".to_string(),
        });
        let serialized = serde_json::to_string(&a).unwrap();
        println!("{}", serialized);

        // let b = MyEnum::B("test".to_string());
        // let serialized = serde_json::to_string(&b).unwrap();
        // println!("{}", serialized);

        let c = MyEnum::C;
        let serialized = serde_json::to_string(&c).unwrap();
        println!("{}", serialized);
    }

    #[test]
    fn get_dongle_request() {
        let request = DongleRequest::GetPasswordDescriptionsAndSwitchReader;
        let serialized = serde_json::to_string(&request).unwrap();
        println!("{}", serialized);
    }

    #[test]
    fn test_errors() {
        let err: Result<(), String> = Result::Err("wasspoppin".to_string());
        println!("Printing le error: {:?}", err);

        let err: () =
            Result::Err("wasspoppin".to_string()).unwrap_or_else(|e| eprintln!("fugg: {}", e));
    }
    #[test]
    fn test_port_cdc() {
        let port_path = "/dev/ttyACM0";
        let mut port = get_port_instance(port_path).unwrap();
        let card = Card {
            name: String::from("VeryLongNameStringVeryLongNameStringVeryLongNameStringVeryLongNameStringVeryLongNameStringVeryLongNameStringVeryLongNameStringVeryLongNameStringVeryLongNameStringVeryLongNameStringVeryLongNameString"),
            password: String::from("VeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordStringVeryLongPasswordString"),
            rfid: String::from("VeryLongRfidStringVeryLongRfidStringVeryLongRfidStringVeryLongRfidStringVeryLongRfidStringVeryLongRfidStringVeryLongRfidStringVeryLongRfidStringVeryLongRfidStringVeryLongRfidStringVeryLongRfidString"),
        };
        let req = DongleRequest::NewCard(card);

        let send = serde_json::to_string(&req).unwrap();
        port.write_all(send.as_bytes());
    }

    #[test]
    fn thread_join_test() {
        let handle = std::thread::spawn(|| {
            println!("Hello from thread first");
            std::thread::sleep(std::time::Duration::from_secs(2));

            println!("Hello from thread second");
        });
        // This proves we dont *need* to join a thread, the resources will be cleaned up by return
        // Not a feature of RAII but just dont need to call join
        // handle.join().unwrap();

        println!("Hello from main");

        std::thread::sleep(std::time::Duration::from_secs(4));

        println!("Hello from main last");
    }

    // fn mutex_test() {
    //     use std::sync::{Arc};
    //     use tokio::sync::Mutex;
    //     let data = Arc::new(Mutex::new(0));
    //     let data_clone = data.clone();
    //     let handle = std::thread::spawn(move || {
    //         let mut data = data_clone.lock().unwrap();
    //         *data += 1;
    //     });
    //     handle.join().unwrap();
    //     println!("{:?}", data);
    // }
}
