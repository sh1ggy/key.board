'use client';
import { Navbar } from "@/components/Navbar";
import { useContext, useEffect, useRef, useState } from "react";
import { useRouter } from 'next/navigation';
import { useToast } from "@/hooks/useToast";
import { reflashPartition } from "@/lib/services";
import { DongleStateContext, DongleState, NewCardsContext, PortContext } from "./_app";
import CommandTerminal from "@/components/CommandTerminal";
import { Command } from "@tauri-apps/api/shell";
import { useError } from "@/hooks/useError";
import type { UnlistenFn } from "@tauri-apps/api/event"
import { sleep } from "@/lib/utils";


const eyeOffIcon = '/eyeOff.svg'
const eyeOnIcon = '/eyeOn.svg'

export default function CreateCard() {
	const setToast = useToast();
	const setError = useError();

	const [name, setName] = useState("");
	const [password, setPassword] = useState("");
	const [showPassword, setShowPassword] = useState(false);
	const [isLoading, setIsLoading] = useState(false);

	const loadingBinaryCommand = useRef<Command | null>(null);
	// This is responsible for tracking that the command is actually running a command as opposed to loading 
	const [isRunningCommand, setRunningCommand] = useState<boolean>(false);

	const [rfid, setRfid] = useState<string>("");
	const [serverRfid, setServerRfid] = useState<string>("");

	const router = useRouter();
	const [selectedPort, setSelectedPort] = useContext(PortContext);
	const rfidEventUnlisten = useRef<UnlistenFn | null>(null);
	const [currBin, setCurrentBin] = useContext(DongleStateContext);

	const init = async () => {
		// Check here if the binary has already been loaded, start up the server
		if (currBin == DongleState.CardReader) {
			const invoke = (await import('@tauri-apps/api')).invoke;

			const listenServer = await invoke('start_listen_server', { "port": selectedPort });
			console.log({ listenServer });

			rfidEventUnlisten.current = await listen<string>("rfid", (e) => {
				console.log(e.payload);
				setServerRfid(e.payload);
			})
		}
	}

	const unMount = async () => {
		const invoke = (await import('@tauri-apps/api')).invoke;
		if (rfidEventUnlisten.current)
			rfidEventUnlisten.current();
		const stopServerRes = await invoke('stop_listen_server');
		console.log({ stopServerRes });
	}

	useEffect(() => {
		init();
		return () => {
			unMount();
		}
	}, []);

	useEffect(() => {
		// New rfid detected
		if (serverRfid != rfid) {
			setToast(`Detected new RFID: ${serverRfid}`);
			setRfid(serverRfid)
		}
	}, [serverRfid]);

	const createCard = async (name: string, password: string) => {
		if (rfid == "") {
			setError("RFID UID not detected");
			return;
		}
		if (name == "") {
			setError("Enter name");
			return
		};
		if (password == "") {
			setError("Enter password");
			return
		};

		const newCard: Card = {
			name,
			password,
			rfid,
		}
		
		let exitEarly = false;
		setNewCards((prev) => {
			for (const card of prev) {
				if (name === card.name) {
					console.log("dupe");
					setError(`Duplicate card name ${name}`);
					exitEarly = true;
					return prev;
				}
			}
			const tempCards = [...prev, newCard];
			return tempCards;
		});

		if (exitEarly) return;

		setToast("Card created!");
		router.push("/");

	}

	const onLoadReaderBin = async () => {
		// LOAD CARD READER BINARY HERE
		setIsLoading(true); // disabling all input

		const Command = (await import('@tauri-apps/api/shell')).Command;
		const invoke = (await import('@tauri-apps/api')).invoke;
		const listen = (await import('@tauri-apps/api')).event.listen;
		const path = (await import('@tauri-apps/api')).path;
		const stopServerRes = await invoke('stop_listen_server');
		console.log({ stopServerRes });


		const bootLoaderPath = await path.resolveResource("bin/arduino-bins/boot_app0.bin");
		const bootLoaderQioPath = await path.resolveResource("bin/arduino-bins/bootloader_qio_80m.bin");
		const rfidPath = await path.resolveResource("bin/arduino-bins/read_rfid.ino.bin");
		const rfidPartitionPath = await path.resolveResource("bin/arduino-bins/read_rfid.ino.partitions.bin");

		// const file = await path.resolveResource("bin/arduino-bins/binariesSource.txt");
		// const resourceDirPath = await path.resourceDir();
		// console.log({binResourcePath, fake, resourceDirPath});
		// console.log("thing", await window.__TAURI__.fs.readTextFile(file));

		// Resources are loaded in a path that is referenced in the same way it is stated in resources. eg. debug/bin/arduino-bins/bin
		loadingBinaryCommand.current = Command.sidecar("bin/dist/esptool", [
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
			rfidPath,
			`0x8000`,
			rfidPartitionPath
		]);
		setRunningCommand(true);
		const res = await loadingBinaryCommand.current.execute();
		if (res.stdout.includes("the port doesn't exist")) {
			setError(`The port ${selectedPort} isn't available or is already in use by another process`);
			setRunningCommand(false);
			setIsLoading(false);
			return;
		}
		console.log({ res });
		setCurrentBin(DongleState.CardReader);
		setToast(`Loaded CardReader binary, starting reader server`);
		setRunningCommand(false);
		setIsLoading(false);
		await sleep(500);

		const listenServer = await invoke('start_listen_server', { "port": selectedPort });
		console.log({ listenServer });

		rfidEventUnlisten.current = await listen<string>("rfid", (e) => {
			console.log(e.payload);
			setServerRfid(e.payload);
		})
		// -- Removing this because of the notification that shows up later 
		// setToast("You may have to reboot the device once to make it work!")

		// 		C:\Users\anhad\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6/tools/partitions/boot_app0.bin 0x1000 C:\Users\anhad\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6/tools/sdk/bin/bootloader_qio_80m.bin 0x10000 C:\Users\anhad\AppData\Local\Temp\arduino_build_509800/read_rfid.ino.bin 0x8000 C:\Users\anhad\AppData\Local\Temp\arduino_build_509800/read_rfid.ino.partitions.bin
	}

	return (
		<>
			<button
				onClick={() => router.push("/")}
				className="text-gray text-left p-3 bg-[#213352] w-full text-[white] focus:outline-none focus:ring-[#213352]">Back
			</button>
			<div className="select-none justify-end text-center text-white w-full text-xl py-6 px-3 bg-[#454444]"><strong>Create Card</strong></div>
			<div className='flex flex-col h-[75vh] align-middle w-full p-3 items-center justify-center bg-[#5D616C]'>
				<div className="flex flex-col p-9 rounded-lg bg-[#292828]">
					<form
						onSubmit={(e) => {
							e.preventDefault();
							createCard(name, password)
						}}
						className="flex flex-col items-center"
					>
						<code className='bg-[#8F95A0] cursor-pointer transition duration-300 hover:scale-95 rounded-lg p-3 mt-3 mb-3'>
							{currBin == DongleState.CardReader &&
								<strong>UID: {!rfid ? "N/A" : rfid}</strong>
							}
							{currBin != DongleState.CardReader &&
								<>
									<strong>UID: </strong>
									<input
										type="text"
										disabled={isLoading}
										placeholder="enter UID..."
										className="input bg-inherit focus:outline-none focus:ring-slate text-white placeholder-white px-3 rounded-lg"
										onChange={e => { setRfid(e.target.value) }}
										value={rfid}
									/>
								</>
							}
						</code>
						<div className='flex flex-row items-center'>
							<input
								type="text"
								disabled={isLoading}
								placeholder="enter name..."
								className="input bg-white focus:outline-none focus:ring-slate text-dim-gray py-3 pl-3 pr-[3.75rem] m-3 rounded-lg"
								onChange={e => { setName(e.target.value) }}
							/>
						</div>
						<div className='flex flex-row items-center'>
							<input
								type={`${showPassword ? 'text' : 'password'}`}
								disabled={isLoading}
								placeholder="enter password..."
								className="input focus:outline-none focus:ring-slate bg-white text-dim-gray p-3 mb-3 rounded-l-lg"
								onChange={e => { setPassword(e.target.value) }}
							/>
							<button
								onClick={(e) => {
									setShowPassword(!showPassword);
									e.preventDefault();
								}}
								type={"button"}
								disabled={isLoading}
								className="inline-flex focus:outline-none focus:ring-slate text-sm font-medium text-center items-center px-3 py-3 mb-3 text-white bg-white rounded-r-lg">
								{showPassword ?
									<img className='object-contain w-6 h-6 items-center' src={eyeOnIcon} />
									:
									<img className='object-contain w-6 h-6 items-center' src={eyeOffIcon} />
								}
							</button>
						</div>
						<div className="flex flex-row items-center justify-center">
							<label htmlFor="create-card-modal" className="btn btn-ghost">
								{currBin != DongleState.CardReader &&
									<>
										<button
											onClick={onLoadReaderBin}
											type={"button"}
											className="text-gray text-center p-3 m-3 transition duration-300 hover:scale-105 bg-[#454444] rounded-lg text-[white] focus:ring-4 focus:outline-none focus:ring-[#454444]">Load Card Reader Binary
										</button>
									</>
								}
							</label>
						</div>
						<label htmlFor="create-card-modal" className="btn btn-ghost">
							<button
								type={"submit"}
								className="text-gray text-center p-3 m-3 bg-green-600 hover:bg-green-700 focus:ring-4 focus:outline-none focus:ring-green-300 rounded-lg text-[white]">Create Card
							</button>
						</label>
					</form>
					{(currBin == DongleState.CardReader) &&
						<div className="bg-[#8B89AC] rounded-lg p-6 mt-3">
							<h1 className='select-none text-center text-white'><strong>Scan RFID card to input UID</strong></h1>
							<h3 className='select-none text-center text-white text-sm'>Reboot button on ESP32 may need to be pressed</h3>
						</div>
					}
				</div>
			</div >
				<CommandTerminal className={`p-6 ${!isLoading && 'hidden'} flex w-auto text-left`} commandObj={loadingBinaryCommand} enabled={isRunningCommand} />
		</>
	)
}
