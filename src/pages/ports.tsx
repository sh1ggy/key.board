'use client';
import { useContext, useEffect, useRef, useState } from "react";
import { getCardsDb, getCurrentWorkingDir, getEspBinDir, getPorts, getReadBinDir, startImports, startlistenServer, test } from "@/lib/services";
import { DongleStateContext, DongleState, LoadedCardsContext, NewCardsContext, PortContext } from "./_app";
import { useToast } from '@/hooks/useToast';
import { useRouter } from 'next/navigation';
import React from "react";
import CommandTerminal from "@/components/CommandTerminal";
import type { Command } from '@tauri-apps/api/shell';
import { Card } from ".";
import { useError } from "@/hooks/useError";
import { truncateString } from "@/lib/utils";
import { invoke } from "@tauri-apps/api";

type InternalValTypes =
	"STR" |
	"U32" |
	"BLOB";


type InternalVal = {
	type: InternalValTypes,
	value: string
}

type DbType = Record<string, Record<string, InternalVal> & { num_cards: { type: "U32", value: string }, uids: { type: "BLOB", value: string } }>;


export default function PortSelection() {
	const [ports, setPorts] = useState<string[]>([]);
	const [selectedPort, setSelectedPort] = useContext(PortContext);
	const setToast = useToast();
	const router = useRouter();
	const getDataCommand = useRef<Command | null>(null);
	const [isRunningCommand, setRunningCommand] = useState<boolean>(false);
	const [cards, setCards] = useContext(LoadedCardsContext);
	const [newCards, setNewCards] = useContext(NewCardsContext);
	const [currBin, setCurrentBin] = useContext(DongleStateContext);
	const setError = useError();

	useEffect(() => {
		const init = async () => {
			await startImports();


			const recvPorts = await invoke<string[]>('get_ports');
			setPorts(recvPorts);
			if (recvPorts.length != 0)
				setSelectedPort(recvPorts[0]);
		}
		init();
		// router.push("/");
	}, [])

	const proceedToCardsScreen = async () => {
		if (selectedPort == null) {
			setError("Select a port first");
			return;
		}

		// const Command = (await import('@tauri-apps/api/shell')).Command;

		// const espBin = await getEspBinDir();
		// let readFileBin = await getReadBinDir();

		// // Not needed as execute also submits events for stdout and is more async await agnostic
		// // const childProcess = await getDataCommand.current.spawn();
		// setRunningCommand(true);

		// let res = await getDataCommand.current.execute();
		// if (res.code != 0) {
		// 	const bootModeErrorString = "Wrong boot mode detected (0x13)";
		// 	if (res.stdout.includes(bootModeErrorString)) {
		// 		// Ping the user that they need to hold down the boot button and try again
		// 		setError(`You have a buggy ESP32. \
		// 		Please hold down the BOOT button while the terminal is running commands, or while \`Serial port ${selectedPort}\` is showing`);
		// 	}
		// 	else {
		// 		setError(`Invalid ESP32 port detected, please select a valid port to connect to`);
		// 		// router.push("/");
		// 	}
		// 	setRunningCommand(false);
		// 	return;

		// }


		// let readFileBin = String.raw`C:\Users\anhad\AppData\Local\com.kongi.dev\1683189187486_data.bin`

		// let db: DbType;

		// try {
		// 	db = JSON.parse(analyzeRes.stdout) as DbType;
		// }
		// catch {
		// 	setError("Database may be corrupt on device, starting new DB", `DbParse result: ${truncateString(analyzeRes.stdout, 150)}`);
		// 	setCards([]);
		// 	setRunningCommand(false);
		// 	router.push("/");
		// 	return;
		// }
		// const nameSpace = db['kb'];
		// if (!nameSpace) {
		// 	setError("No previous DB found of ESP, starting empty DB [Namespace KB not found]")
		// 	setCards([]);
		// 	setRunningCommand(false);
		// 	router.push("/");
		// 	return;
		// }
		// console.log({ db });
		// const splitUids = nameSpace.uids.value.trim().split(' ');
		// const unflattenedUids: String[][] = [];

		// for (let i = 0; i < splitUids.length; i += 4) {
		// 	unflattenedUids.push(splitUids.slice(i, i + 4));
		// }
		// const gottenCards: Card[] = [];

		// for (let i = 0; i < parseInt(nameSpace.num_cards.value); i++) {
		// 	gottenCards.push({
		// 		name: nameSpace[`name${i}`].value,
		// 		password: nameSpace[`pass${i}`].value,
		// 		rfid: unflattenedUids[i].join('').toLowerCase(),
		// 	});
		// }
		console.log(await getCardsDb());

		setToast("Finished loading data from ESP!");
		// setCards(gottenCards);
		// setNewCards(gottenCards);

		router.push("/");

		// getDataCommand.current.on('close', () => {
		// 	console.log(`Saved BinaryFile in: ${readFileBin}`);
		// 	setToast(`Saved BinaryFile in: ${readFileBin}`);
		// 	setRunningCommand(false);
		// })


		// await startlistenServer(selectedPort);


	}


	return (
		<>
			<button
				onClick={async () => {
					const recvPorts = await invoke<string[]>('get_ports');
					// const recvPorts = await getPorts();
					setPorts(recvPorts);
				}}
				className="flex px-2 text-sm font-medium text-right justify-end w-full text-white bg-black py-3">
				Force Detect Ports
			</button>
			<div className="flex flex-col items-center bg-[#292828] h-full w-full">
				<div className="justify-center text-white w-full text-xl py-6 px-3 bg-[#213352]"><strong>Port Selection</strong></div>
				<ul className="text-sm text-black w-full bg-[#51555D]" aria-labelledby="dropdownDefaultButton">
					{
						(ports.length == 0) ?
							<li>
								<a className="select-none block w-full px-3 py-2 text-white bg-gray-500">No ports</a>
							</li>
							:
							ports.map((p, i) => {
								return (
									<>
										{(selectedPort == p) ?
											<li key={i} className="cursor-pointer">
												<a className="select-none block w-full px-3 py-2 bg-gray-500 text-white" onClick={() => {
													setSelectedPort(p);
												}}>{p}</a>
											</li>
											:
											<li key={i} className="cursor-pointer">
												<a className="select-none block w-full px-3 py-2 text-white hover:bg-gray-100 dark:hover:bg-gray-600 dark:hover:text-white active:animate-pulse" onClick={() => {
													setSelectedPort(p);
												}}>{p}</a>
											</li>
										}
									</>
								)
							})
					}
				</ul>
				<code className='bg-[#8F95A0] w-full p-3 px-3 text-sm'><strong>Selected Port: </strong>{!selectedPort ? "N/A" : selectedPort}</code>
				<button
					disabled={isRunningCommand}
					onClick={proceedToCardsScreen}
					className="flex disabled:bg-green-800 disabled:cursor-not-allowed disabled:text-slate focus:ring-4 focus:outline-none focus:ring-green-300 text-sm p-3 font-medium text-center items-center justify-center w-full text-white bg-green-600 hover:bg-green-700 py-3">
					Connect to Device
				</button>
				<CommandTerminal enabled={isRunningCommand} className="p-6 flex w-auto bg-transparent" commandObj={getDataCommand} />
			</div>
		</>
	)
}
