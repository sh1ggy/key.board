import CommandTerminal from '@/components/CommandTerminal';
import { useError } from '@/hooks/useError';
import { useToast } from '@/hooks/useToast';
import { Command } from '@tauri-apps/api/shell';
import { useRouter } from 'next/navigation';
import { useContext, useEffect, useRef, useState } from 'react';
import { DongleStateContext, DongleState, PortContext } from './_app';

export default function ActiveView() {

	const [isRunningCommand, setRunningCommand] = useState<boolean>(false);
	const loadMainBinCommand = useRef<Command | null>(null);
	const [selectedPort, setSelectedPort] = useContext(PortContext);
	const [currBin, setCurrentBin] = useContext(DongleStateContext);
	const setToast = useToast();
	const setError = useError();
	const router = useRouter();

	useEffect(() => {
		setToast("Loading the key binary...");

		// C:\Users\anhad\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\3.0.0/esptool.exe --chip esp32 --port COM4 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0xe000 C:\Users\anhad\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6/tools/partitions/boot_app0.bin 0x1000 C:\Users\anhad\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6/tools/sdk/bin/bootloader_qio_80m.bin 0x10000 C:\Users\anhad\AppData\Local\Temp\arduino_build_422462/main_bt.ino.bin 0x8000 C:\Users\anhad\AppData\Local\Temp\arduino_build_422462/main_bt.ino.partitions.bin 
		// onLoadMainBin();

	}, []);

	const loadMainBin = async () => {
		setToast("Loading the key binary...");
		setRunningCommand(true);

		const Command = (await import('@tauri-apps/api/shell')).Command;
		const invoke = (await import('@tauri-apps/api')).invoke;
		const listen = (await import('@tauri-apps/api')).event.listen;
		const path = (await import('@tauri-apps/api')).path;

		const bootLoaderPath = await path.resolveResource("bin/arduino-bins/boot_app0.bin");
		const bootLoaderQioPath = await path.resolveResource("bin/arduino-bins/bootloader_qio_80m.bin");
		const mainPath = await path.resolveResource("bin/arduino-bins/main_bt.ino.bin");
		const mainPartitionPath = await path.resolveResource("bin/arduino-bins/main_bt.ino.partitions.bin");


		loadMainBinCommand.current = Command.sidecar("bin/dist/esptool", [
			`--chip`,
			`esp32`,
			`--port`,
			selectedPort!,
			`--baud`,
			`921600`,
			`--before`,
			`default_reset`,
			`--after`,
			`hard_reset`,
			`write_flash`,
			`-z`,
			`--flash_mode`,
			`dio`,
			`--flash_freq`,
			`80m`,
			`--flash_size`,
			`detect`,
			`0xe000`,
			`${bootLoaderPath}`,
			`0x1000`,
			bootLoaderQioPath,
			`0x10000`,
			mainPath,
			`0x8000`,
			mainPartitionPath
		]);
		const res = await loadMainBinCommand.current.execute();
		console.log({ res });


		if (res.code != 0) {
			const bootModeErrorString = "Wrong boot mode detected (0x13)";
			if (res.stdout.includes(bootModeErrorString)) {
				// Ping the user that they need to hold down the boot button and try again
				setError(`You have a buggy ESP32. \
				Please hold down the BOOT button while the terminal is running commands, or while \`Serial port ${selectedPort}\` is showing`);
			}
			else if (res.stdout.includes("the port doesn't exist")) {
				setError(`The port ${selectedPort} isn't available or is already in use by another process`);
			}
			else {
				setError(`Code did not run successfully`, `Error: ${res.stderr}`);
			}
			setRunningCommand(false);
			return;
		}

		if (res.stderr !== "") {
			setError("Error loading binary", res.stderr);
			setRunningCommand(false);
			return;
		}
		setRunningCommand(false);
		setCurrentBin(DongleState.CardReader);
		setToast("Successfully loaded the key binary!");

		router.push("/");
	}

	return (
		<>
			<button
				onClick={() => router.push("/")}
				className="text-gray text-left p-3 mb-24 bg-[#213352] w-full text-[white]">Back
			</button>

			<div className="flex flex-col items-center bg-[#292828] h-full w-full justify-center">
				<div className="justify-center text-4xl animate-pulse items-center rounded-lg text-green-700">
					<strong>Loading key binary!</strong>
				</div>
				<CommandTerminal
					enabled={isRunningCommand}
					className="p-6 flex w-auto text-left"
					commandObj={loadMainBinCommand}
					onLoaded={loadMainBin}
				/>
			</div>
		</>
	)
}