import { Card } from "@/pages/main";
import { useContext, useEffect, useState } from "react";
import { NewCardsContext } from "@/pages/_app";
import { useToast } from "@/hooks/useToast";
import { usePrevious } from "@/hooks/usePrevious";
import { useRouter } from "next/router";
import { useError } from "@/hooks/useError";

const eyeOffIcon = '/eyeOff.svg'
const eyeOnIcon = '/eyeOn.svg'

export default function EditView() {
	const saveCard = (i: number) => {
		let exitEarly = false;
		setCards((prev) => {
			const editCard: Card = {
				name: prev[i].name,
				password: password,
				rfid: prev[i].rfid,
			}
			const tempCards = [...prev];
			tempCards.splice(i, 1, editCard);
			setToast("Card saved!");
			return tempCards;
		});
		if (exitEarly) return;
		router.push('/');
	}

	const setToast = useToast();
	const [cards, setCards] = useContext(NewCardsContext);
	const router = useRouter();

	const index = parseInt(router.query.id as string);
	const [password, setPassword] = useState("");
	const [showPassword, setShowPassword] = useState(false);
	const setError = useError();

	useEffect(() => {
		setPassword(cards[index].password);
	}, [cards])

	return (
		<>
			<div className='flex flex-col mx-6 items-center h-screen overflow-hidden justify-center'>
				<button
					onClick={() => router.push("/")}
					className="w-full absolute top-0 inline-flex px-3 py-2 text-sm font-medium text-center text-white bg-[#213352]">
					Back
				</button>
				<div className="justify-end absolute top-9 text-center text-white w-full text-xl py-6 px-3 bg-[#454444] break-all"><strong>Editing {cards[index]?.name}</strong></div>
				{/* <div className="justify-center text-white text-xl p-6 bg-[#213352] rounded-t-lg">Editing Card</div> */}
				<div className="justify-center text-white p-6 bg-[#5D616C] items-center rounded-lg ">
					<div className="my-6">
						<code className='text-sm tracking-tight text-gray-900 bg-[#8F95A0] rounded-lg p-3 mb-3'>
							<strong>ID: </strong>{cards[index]?.rfid}
						</code>
					</div>
					<form
						onSubmit={(e) => {
							e.preventDefault();
							saveCard(index);
						}}
						className="flex flex-col items-center"
					>
						<div className="input w-full max-w-xs bg-[#747986] text-white text-dim-gray p-3 mb-3 break-all border-neutral-300 rounded-lg">
							<strong>Name: </strong>{cards[index]?.name}
						</div>
						<div className='flex flex-row items-center pb-3'>
							<input
								type={`${showPassword ? 'text' : 'password'}`}
								placeholder="enter password..."
								className="input w-full focus:outline-none max-w-xs bg-white text-black text-dim-gray p-3 rounded-l-lg"
								onChange={e => { setPassword(e.target.value) }}
							/>
							<button
								onClick={() => {
									setShowPassword(!showPassword);
								}}
								type={"button"}
								className="inline-flex focus:outline-none text-sm font-medium text-center h-full items-center rounded-r-lg text-white bg-white p-3">
								{showPassword ?
									<img className='object-contain w-6 h-6 items-center' src={eyeOnIcon} />
									:
									<img className='object-contain w-6 h-6 items-center' src={eyeOffIcon} />
								}
							</button>
						</div>
						<label htmlFor="create-card-modal" className="btn btn-ghost">
							<button
								onClick={() => () => saveCard(index)}
								className="text-gray text-center p-3 m-3 bg-green-600 hover:bg-green-700 focus:ring-4 focus:outline-none focus:ring-green-300 rounded-lg text-[white]">Save Card
							</button>
						</label>
					</form>
				</div>
			</div>
		</>
	)
}