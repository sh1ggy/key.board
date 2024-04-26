'use client';
import { Navbar } from "@/components/Navbar";
import { useContext, useEffect, useRef, useState } from "react";
import { useRouter } from 'next/navigation';
import { useToast } from "@/hooks/useToast";
import { deleteCard } from "@/lib/services";
import { DongleStateContext, DongleState, PortContext, CardsContext } from "./_app";
import CommandTerminal from "@/components/CommandTerminal";
import { Command } from "@tauri-apps/api/shell";
import { useError } from "@/hooks/useError";
import type { UnlistenFn } from "@tauri-apps/api/event"
import { sleep } from "@/lib/utils";


const eyeOffIcon = '/eyeOff.svg'
const eyeOnIcon = '/eyeOn.svg'

interface NewCard {
	name: string;
	password: string;
}

interface RFIDEvent {
	rfid: string;
}

export default function CreateCard() {
	const setToast = useToast();
	const setError = useError();

	const [name, setName] = useState("");
	const [password, setPassword] = useState("");
	const [showPassword, setShowPassword] = useState(false);
	const [isLoading, setIsLoading] = useState(false);
	const [cards, setCards] = useContext(CardsContext);

	const [rfid, setRfid] = useState<string>("");
	const [serverRfid, setServerRfid] = useState<string>("");

	const router = useRouter();
	const [selectedPort, setSelectedPort] = useContext(PortContext);
	const rfidEventUnlisten = useRef<UnlistenFn | null>(null);
	const [currBin, _] = useContext(DongleStateContext);

	const init = async () => {
		// Check here if the binary has already been loaded, start up the server
		console.log(currBin);

		if (currBin == DongleState.CardReader) {
			const listen = (await import('@tauri-apps/api')).event.listen;
			console.log("Listening for RFID events");

			rfidEventUnlisten.current = await listen<RFIDEvent>("rfid", (e) => {
				console.log(e.payload);
				setServerRfid(e.payload.rfid);
			})
		}
		else {
			// router.push("/");
		}
	}


	const unMount = async () => {
		if (rfidEventUnlisten.current)
			rfidEventUnlisten.current();
	}

	useEffect(() => {
		init();
		return () => {
			unMount();
		}
	}, []);

	//Were gonna use this as opposed to regenerating the callback cuz idk how to do that its been 5 years facebook please
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

		const newCard: NewCard = {
			name,
			password,
		}

		let exitEarly = false;
		setCards((prev) => {
			for (const card of prev) {
				if (name === card.name) {
					console.log("dupe");
					setError(`Duplicate card name ${name}`);
					exitEarly = true;
					return prev;
				}
			}
			const tempCards = [...prev, { name, rfid }];
			return tempCards;
		});

		if (exitEarly) return;

		//Send the newCard to the server
		try {
			const invoke = (await import('@tauri-apps/api')).invoke;
			const resp = await invoke('send_new_card', { card: newCard });

			setToast("Card created!");
			router.push("/main");

		}
		catch (e: any) {
			setError("Error creating card", e);
			//Delete the card from the local state
			setCards((prev) => {
				const tempCards = [...prev];
				tempCards.pop();
				return tempCards;
			})
			throw e;
		}


	}


	return (
		<>
			<button
				onClick={() => router.push("/main")}
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
							<strong>UID: {!rfid ? "N/A" : rfid}</strong>
						</code>
						<div className='flex flex-row items-center'>
							<input
								type="text"
								value={name}
								placeholder="enter name..."
								className="input bg-white focus:outline-none focus:ring-slate text-dim-gray py-3 pl-3 pr-[3.75rem] m-3 rounded-lg"
								onChange={e => { setName(e.target.value) }}
							/>
						</div>
						<div className='flex flex-row items-center'>
							<input
								type={`${showPassword ? 'text' : 'password'}`}
								value={password}
								placeholder="enter password..."
								className="input focus:outline-none focus:ring-slate bg-white text-dim-gray p-3 rounded-l-lg"
								onChange={e => { setPassword(e.target.value) }}
							/>
							<button
								onClick={(e) => {
									e.preventDefault();
									setShowPassword((prev) => !prev);
								}}
								type={"button"}
								className="px-3 py-3 bg-white text-gray-700 rounded-r-lg focus:outline-none focus:ring-slate">
								{showPassword ?
									<img className='object-contain w-6 h-6 items-center' src={eyeOnIcon} />
									:
									<img className='object-contain w-6 h-6 items-center' src={eyeOffIcon} />
								}
							</button>
						</div>
						<label htmlFor="create-card-modal" className="btn btn-ghost">
							<button
								type={"submit"}
								className="text-gray text-center p-3 m-3 bg-green-600 hover:bg-green-700 focus:ring-4 focus:outline-none focus:ring-green-300 rounded-lg text-[white]">Create Card
							</button>
						</label>
					</form>

					<div className="bg-[#8B89AC] rounded-lg p-6 mt-3">
						<h1 className='select-none text-center text-white'><strong>Scan RFID card to input UID</strong></h1>
						{/* <h3 className='select-none text-center text-white text-sm'>Reboot button on ESP32 may need to be pressed</h3> */}
					</div>
				</div>
			</div >
		</>
	)
}
