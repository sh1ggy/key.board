import { useToast } from "@/hooks/useToast";
import { CardsContext} from "@/pages/_app";
import { deleteCard } from "@/lib/services";
import { useRouter } from "next/router";
import { useContext } from "react";
import { PasswordLessCard } from "@/lib/models";

const deleteIcon = '/delete.svg'
const editIcon = '/edit.svg'

export interface CardsViewProps {
	card: PasswordLessCard,
	cardIndex: number,
}

export function CardsView({ cardIndex, card }: CardsViewProps) {
	const [cards, setCards] = useContext(CardsContext);
	const deleteCard = async (i: number) => {
		setCards((prev) => {
			const tempCards = [...prev];
			tempCards.splice(i, 1);
			return tempCards;
		})

		// if (await deleteCard())
		// 	setToast("Card deleted!");
	}

	const setToast = useToast();
	const router = useRouter();

	return (
		<div className="flex flex-col w-96 h-64 p-6 justify-items-end bg-[#5D616C] rounded-lg mt-24 mx-6">
			<div className='flex flex-col items-start'>
				<code className="mb-3 text-sm font-bold tracking-tight p-1 px-3 rounded-lg bg-[#8F95A0] text-gray-900 dark:text-white"><strong>ID: {card.rfid}</strong></code>
				<h5 className="mb-3 text-2xl font-bold tracking-tight break-all text-gray-900 max-h-9 dark:text-white">{card.name}</h5>
			</div>
			<div className='flex flex-1 items-end justify-end'>
				<button className="px-3 py-2 mr-3 text-sm font-medium text-center text-white rounded-lg bg-green-600 hover:bg-green-700 focus:ring-4 focus:outline-none focus:ring-green-300">
					<img
						onClick={() => {
							router.push(`/edit/${cardIndex}`)
						}}
						className='object-contain w-6 h-6 items-center' src={editIcon} />
				</button>
				<button
					onClick={() => deleteCard(cardIndex)}
					className="px-3 py-2 text-sm font-medium text-center text-white bg-red-700 rounded-lg hover:bg-red-800 focus:ring-4 focus:outline-none focus:ring-red-300 dark:bg-red-600 dark:hover:bg-red-700 dark:focus:ring-red-800">
					<img className='object-contain w-6 h-6 items-center' src={deleteIcon} />
				</button>
			</div>
		</div>
	)
}