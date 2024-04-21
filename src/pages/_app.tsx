import React, { ReactNode, useEffect, useState, FunctionComponent } from 'react';
import GlobalToastProvider from '@/components/GlobalToastProvider';
import GlobalErrorProvider from '@/components/GlobalErrorProvider';
import { useRouter } from 'next/navigation';
import type { AppProps } from 'next/app'
import { Card } from './main';
import Head from 'next/head';
import '@/styles/globals.css'
import '@/styles/xterm.css'


// const CARDS_SEED: Card[] = [{
//   name: "Test",
//   password: "passwordle",
//   rfid: "112233"
// }];

export enum DongleState {
  Unknown,
  CardReader,
  Master
}

// @ts-ignore
export const PortContext = React.createContext<[string | null, React.Dispatch<React.SetStateAction<string | null>>]>(null);
// @ts-ignore
export const LoadedCardsContext = React.createContext<[Card[], React.Dispatch<React.SetStateAction<Card[]>>]>(null);
// @ts-ignore
export const NewCardsContext = React.createContext<[Card[], React.Dispatch<React.SetStateAction<Card[]>>]>(null);
// @ts-ignore
export const DongleStateContext = React.createContext<[DongleState, React.Dispatch<React.SetStateAction<DongleState>>]>(null);

const initialDongleState = DongleState.CardReader;

export default function App({ Component, pageProps }: AppProps) {
  const portState = useState<string | null>(null);
  const binaryState = useState<DongleState>(initialDongleState);
  const cardsState = useState<Card[]>([]);
  const newCardsState = useState<Card[]>([]);
  const router = useRouter();

  useEffect(() => {
    // if (portState[0] == null) router.push("/ports");
  }, [])

  return (
    // <SafeHydrate> Doesnt work lol
    <GlobalToastProvider>
      <GlobalErrorProvider>
        <PortContext.Provider value={portState}>
          <LoadedCardsContext.Provider value={cardsState}>
            <NewCardsContext.Provider value={newCardsState}>
              <DongleStateContext.Provider value={binaryState}>
                <Head>
                  <script src="http://localhost:8097"></script>
                </Head>
                <Component {...pageProps} />
              </DongleStateContext.Provider>
            </NewCardsContext.Provider>
          </LoadedCardsContext.Provider>
        </PortContext.Provider>
      </GlobalErrorProvider>
    </GlobalToastProvider>
  )
}