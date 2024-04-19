import { useCallback, useContext, useEffect, useMemo, useState } from 'react'
import { useRouter } from 'next/navigation';

// import { listen } from '@tauri-apps/api/event'
// import { invoke } from '@tauri-apps/api/tauri'
// import { Command } from '@tauri-apps/api/shell'
import { getPorts, reflashPartition, test } from '@/lib/services'
import { CardsView, CardsViewProps } from '@/components/CardsView'
import { Navbar } from '@/components/Navbar'
import { DongleStateContext, LoadedCardsContext, NewCardsContext, PortContext } from './_app'
import { useToast } from '@/hooks/useToast';
import { arraysEqual } from '@/lib/utils';
import { DongleState } from './_app';


export interface Card {
  name: string;
  password: string;
  rfid: string;
}

interface error {
  title: string;
  message: string;
}


function App() {
  // const [rfidPayload, setRfidPayload] = useState<RFIDPayload>({
  //   uid: "",
  //   error: "",
  // });
  const [error, setError] = useState<error>({
    title: "",
    message: "",
  });
  const setToast = useToast();
  const router = useRouter();
  const [selectedPort, setSelectedPort] = useContext(PortContext);
  const [cards, setCards] = useContext(LoadedCardsContext);
  const [newCards, setNewCards] = useContext(NewCardsContext);
  const [binary, setBinary] = useContext(DongleStateContext);

  useEffect(() => {
    // Listen to tauri events

    // const unlistenError = listen<error>("error", (e) => {
    //   console.log(e.payload);
    //   setError(e.payload)
    // })

    // setTimeout(() => init(), 2000);

    return (() => {
      // Unsubscribe from tauri events
      // unlistenRFID;
      // unlistenError;
    })
  }, []);



  const clearData = async () => {
    // const clearData = await invoke('start_listen_server', { "port": selectedPort });
    // await setCards([]);
    setToast("Cards cleared!");
  }

  return (
    <>
      <Navbar />
      <div className={'flex flex-col w-full items-center min-h-screen bg-[rgb(41,40,40)] overflow-hidden'}>
        <div className="flex flex-col w-full items-center p-9 bg-[#5D616C] rounded-b-lg">
          <code className='bg-[#373a41] p-3 my-3 rounded-lg text-[#F7C546]'>
            <strong>Loaded Binary:</strong> {DongleState[binary]}
          </code>
          <div className='flex flex-row my-8'>
            <code
              onClick={() => {
                setSelectedPort(null);
                router.push('/ports');
              }}
              className='cursor-pointer transition duration-300 hover:scale-105 bg-[#8F95A0] p-3 rounded-lg'>
              <strong>Port Selected: </strong>{selectedPort}
            </code>
            {binary != DongleState.Master &&
              <>
                <button
                  className="text-gray cursor-pointer transition duration-300 hover:scale-105 text-center p-3 ml-5 focus:ring-4 focus:outline-none focus:ring-green-300 bg-green-600 rounded-lg text-white"
                  onClick={() => {
                    router.push('/load-main');
                  }}>
                  Load Key Binary
                </button>
              </>
            }
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
          {newCards.length == 0 ?
            <div className="pt-24 text-white">No cards!
            </div>
            :
            <div className='flex flex-wrap items-center justify-center'>
              {newCards.map((c, i) => {
                return (
                  <CardsView key={i} card={c} cardIndex={i} />
                )
              })}
            </div>
          }
        </div>
      </div >
    </>
  )
}

export default App
