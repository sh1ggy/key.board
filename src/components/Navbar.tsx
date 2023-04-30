export interface NavbarProps {
	syncData: () => Promise<void>,
	clearData: () => Promise<void>,
}

export function Navbar(props: NavbarProps) {
	const syncData = props.syncData;
	const clearData = props.clearData;
	
	return (
		<ul className="flex bg-[#8C89AC] py-3 z-10 items-center">
			<li className="text-center flex-1">
				<button className="text-gray text-center p-3 bg-[#292828] rounded-lg text-[white]" onClick={syncData}>Sync</button>
			</li>
			<li className="flex-1 mr-2">
				<div className="flex-1 flex justify-center mr-auto ml-auto navbar-center">
					<img className='object-contain select-none' src="/wlogo.svg" />
				</div>
			</li>
			<li className="text-center flex-1">
				<button className="text-gray text-center p-3 bg-[#292828] rounded-lg text-[white]" onClick={clearData}>Clear Data</button>
			</li>
		</ul>
	)
}