import { use, useCallback, useContext, useEffect, useMemo, useState } from 'react'
import { useRouter } from 'next/navigation';

import { getPorts, deleteCard, test } from '@/lib/services'
import { CardsView, CardsViewProps } from '@/components/CardsView'
import { Navbar } from '@/components/Navbar'
import { DongleStateContext, CardsContext, PortContext } from './_app'
import { useToast } from '@/hooks/useToast';
import { DongleState } from './_app';


interface error {
  title: string;
  message: string;
}


export default function MainPage() {
  const setToast = useToast();
  const router = useRouter();
  const [selectedPort, setSelectedPort] = useContext(PortContext);
  const [cards, setCards] = useContext(CardsContext);
  const [currBin, setCurrentBin] = useContext(DongleStateContext);

  useEffect(() => {
    const init = async () => {

    };

  }, []);

  return (
    <>
      <Navbar />
      <div className={'flex flex-col w-full items-center min-h-screen bg-[rgb(41,40,40)] overflow-hidden'}>
        <div className="flex flex-col w-full items-center p-9 bg-[#5D616C] rounded-b-lg">
          <code className='bg-[#373a41] p-3 my-3 rounded-lg text-[#F7C546]'>
            <strong>Loaded Binary:</strong> {DongleState[currBin]}
          </code>
          <div className='flex flex-row my-8'>
            <code
              onClick={() => {
                setSelectedPort(null);
                router.push('/');
              }}
              className='cursor-pointer transition duration-300 hover:scale-105 bg-[#8F95A0] p-3 rounded-lg'>
              <strong>Port Selected: </strong>{selectedPort}
            </code>
          </div>
          <div className='flex flex-col'>
            <button className="text-gray cursor-pointer transition duration-300 hover:scale-105 ext-center p-3 m-3 bg-[#292828] focus:ring-4 focus:outline-none focus:ring-[#454444] rounded-lg text-white"
              onClick={() => {
                router.push('/create');
              }}>
              Create Card
            </button>
          </div>
        </div>
        <div className='flex flex-row flex-wrap items-center pb-24'>
          {cards.length == 0 ?
            <div className="pt-24 text-white">No cards!
            </div>
            :
            <div className='flex flex-wrap items-center justify-center'>
              {cards.reverse().map((c, i) => {
                return (
                  <CardsView key={i} card={c} cardIndex={cards.length- i} />
                )
              })}
            </div>
          }
        </div>
      </div >
    </>
  )
}


