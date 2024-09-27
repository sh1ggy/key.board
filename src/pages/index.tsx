'use client';
import { useContext, useEffect, useRef, useState } from "react";
import { getCardsDb, getCurrentWorkingDir, getEspBinDir, getPorts, getReadBinDir, startImports, startlistenServer, stoplistenServer, test, testSyncLoop } from "@/lib/services";
import { DongleStateContext, DongleState, CardsContext, PortContext } from "./_app";
import { useToast } from '@/hooks/useToast';
import { useRouter } from 'next/navigation';
import React from "react";
import CommandTerminal from "@/components/CommandTerminal";
import type { Command } from '@tauri-apps/api/shell';
import { useError } from "@/hooks/useError";
import { truncateString } from "@/lib/utils";
// import { invoke } from "@tauri-apps/api";



export default function PortSelection() {
	const [ports, setPorts] = useState<string[]>([]);
	const [selectedPort, setSelectedPort] = useContext(PortContext);
	const setToast = useToast();
	const router = useRouter();
	const getDataCommand = useRef<Command | null>(null);
	const [isRunningCommand, setRunningCommand] = useState<boolean>(false);
	const [cards, setCards] = useContext(CardsContext);
	const [currBin, setCurrentBin] = useContext(DongleStateContext);
	const setError = useError();

	useEffect(() => {
		const init = async () => {
			await startImports();

			// console.log(await invoke<string[]>('get_ports'));
			const recvPorts = await getPorts();
			setPorts(recvPorts);
			if (recvPorts.length != 0)
				setSelectedPort(recvPorts[0]);
		}
		console.log({ cards });

		init();
		// router.push("/main");
	}, [])

	const proceedToCardsScreen = async () => {
		if (selectedPort == null) {
			setError("Select a port first");
			return;
		}
		try {
			await startlistenServer(selectedPort);
			let gottenCards = await getCardsDb();
			console.log({ gottenCards });
			setCards(gottenCards.descriptions);
		}
		catch (e: any) {
			console.error(e);
			setError("Error connecting to ESP32", e);
			return;
		}

		setToast("Finished loading data from ESP!");
		setCurrentBin(DongleState.CardReader);
		router.push("/main");
	}


	return (
		<>
			<button
				onClick={async () => {
					const recvPorts = await getPorts();
					setPorts(recvPorts);
				}}
				className="flex px-2 text-sm font-medium text-right justify-end w-full text-white bg-black py-3">
				Force Refresh Ports
			</button>

			{/* <button
				onClick={async () => {
					await stoplistenServer();
					// await testSyncLoop();
				}}
				className="flex px-2 text-sm font-medium text-right justify-end w-full text-white bg-black py-3">
					Test Sync loop
			</button> */}

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
			</div>
		</>
	)
}
