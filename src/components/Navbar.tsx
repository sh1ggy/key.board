import { useToast } from "@/hooks/useToast";
import { arraysEqual } from "@/lib/utils";
import { CardsContext, NewCardsContext } from "@/pages/_app";
import sync from "@/pages/sync"
import router from "next/router"
import { useContext, useMemo } from "react";


export function Navbar() {
	const [cards, setCards] = useContext(CardsContext);
	const [newCards, setNewCards] = useContext(NewCardsContext);
	const sync = useMemo(() => {
		return !arraysEqual(newCards, cards);
	}, [newCards, cards]);
	const clearData = async () => {
		// const clearData = await invoke('start_listen_server', { "port": selectedPort });
		// await setCards([]);
		setToast("Cards cleared!");
	}
	const setToast = useToast();
	return (

		<ul className="flex bg-[#213352] py-3 z-10 items-center">
			<li className="text-center flex-1">
			</li>
			<li className="flex-1 mr-2">
				<div className="flex-1 flex justify-center mr-auto ml-auto navbar-center">
					<img className='object-contain select-none' src="/wlogo.svg" />
				</div>
			</li>
			<li className="text-center flex-1">
				<button
					onClick={() => { router.push("/sync") }}
					className={`${sync ? 'animate-bounce' : ''} text-gray text-center h-full p-3 m-3 bg-green-600 hover:bg-green-700 focus:ring-4 focus:outline-none focus:ring-green-300 rounded-lg text-[white]`}>
					Sync
				</button>
			</li>
		</ul>
	)
}
