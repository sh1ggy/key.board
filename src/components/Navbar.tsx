import { useToast } from "@/hooks/useToast";
import { arraysEqual } from "@/lib/utils";
import { CardsContext, } from "@/pages/_app";
import sync from "@/pages/sync"
import router from "next/router"
import { useContext, useMemo } from "react";


export function Navbar() {
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
		</ul>
	)
}
