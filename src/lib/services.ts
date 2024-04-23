'use client';
// TODO: organize imports to interfaces (likely make a Card component (Model View Controller pattern))
// import { path, tauri, os } from "@tauri-apps/api";
import type { path as PathType } from '@tauri-apps/api';
import type { invoke as InvokeType } from '@tauri-apps/api/tauri'
import type { listen as ListenType } from '@tauri-apps/api/event'
import { PasswordLessCard } from './models';

export const reflashPartition = async (): Promise<Boolean> => {
  // TODO: await command send
  try {
    // const thing = await invoke('run_thing');
  } catch (error) {
    // Will catch result errors

  }
  // Delete nvc partition command
  // Create new nvc partition
  // Flash nvc partition image
  // Error Checking
  return true;
}

// export const backupCardsToDisk = async (cards: Card[]) => {
//   // Save to localstorage

// }

// Saving the function here makes it reusable across files too
let invoke: typeof InvokeType;
let path: typeof PathType;
let listen: typeof ListenType;
export const startImports = async () => {
  invoke = (await import('@tauri-apps/api')).tauri.invoke;
  path = (await import('@tauri-apps/api')).path;
  listen = (await import('@tauri-apps/api')).event.listen;
}


export const startlistenServer = async (portOption: string) => {
  const listenServer = await invoke('start_listen_server', { "port": portOption });
}
export const stoplistenServer = async () => {
  await invoke('stop_listen_server');
}
export const testSyncLoop = async () => {
  await invoke('test_sync_loop');
}

export const getPorts = async () => {
  return await invoke<string[]>('get_ports');
}

export const getCurrentWorkingDir = async () => {
  return await invoke<string>('get_current_working_dir');
}

export const getCardsDb = async () => {
  return await invoke<PasswordLessCard[]>('get_cards_db');
}

export const getEspBinDir = async () => {
  // Neither of these work on anything but linux
  // const runtimeDir = await path.runtimeDir();
  // const notherLoc = await path.executableDir();

  const workingDir = await getCurrentWorkingDir();
  const espBin = await path.join(workingDir, 'esptool');
  console.log({ workingDir, espBin });

  return espBin;

}
export const getReadBinDir = async () => {
  const appDir = await path.appLocalDataDir();
  // const binFileName = `${Date.now()}_data.bin`;
  // const binDir = await path.join(appDir, binFileName);
  const binDir = await path.join(appDir, `reading.bin`);
  return binDir;
}

export const getTargetPlatformExtension = async () => {
  // let arch = await os.arch();
  // let type = await os.type();
  // switch (type) {
  //   case "Darwin":
  //     return `-${arch}-apple-darwin`
  //   case "Linux":
  //     return `-${arch}-unknown-linux-gnu`
  //   case "Windows_NT":
  //     return `-${arch}-pc-windows-msvc`
  //   default:
  //     throw new Error("Unsupported platform");
  // }
}


export const test = async () => {
  const syncData = await invoke('test');
  console.log({ syncData });
  // await sleep(200);
  // const binaryCommand = new Command(String.raw`C:\Users\anhad\.espressif\python_env\idf5.0_py3.8_env\Scripts\python.exe`,
  //   [String.raw`C:\Users\anhad\esp\esp-idf\components\nvs_flash\nvs_partition_generator\nvs_partition_gen.py`, `generate`, `data.csv`, `data.bin`, `0x5000`]);

  // const binaryChild = await binaryCommand.spawn();
  // const uploadCommand = new Command(
  //   String.raw`C:\Users\anhad\.espressif\python_env\idf5.0_py3.8_env\Scripts\python.exe C:\Users\anhad\esp\esp-idf\components\partition_table\parttool.py`,
  //   [` --port`, `COM4`, `--baud`, `115200`, `write_partition`, `--partition-name=nvs`, `--input`, `"data.bin"`]);

  // binaryCommand.stdout.on('data', line => console.log(`binarycommand stdout: "${line}"`));
  // binaryCommand.stderr.on('data', line => console.log(`binary command stderr: "${line}"`));

  // uploadCommand.stdout.on('data', line => console.log(`uploadcommand stdout: "${line}"`));
  // uploadCommand.stderr.on('data', line => console.log(`uplaodcommand stderr: "${line}"`));

  // binaryCommand.on('close', () => {
  //   uploadCommand.spawn();
  //   console.log("Done");
  // });

  // uploadCommand.on('close', () => {
  //   setToast("Finished saving to disk!");
  // })

}