import React, { ReactNode, useEffect, useState, FunctionComponent } from 'react';
import GlobalToastProvider from '@/components/GlobalToastProvider';
import GlobalErrorProvider from '@/components/GlobalErrorProvider';
import { useRouter } from 'next/navigation';
import type { AppProps } from 'next/app'
import Head from 'next/head';
import '@/styles/globals.css'
import '@/styles/xterm.css'
import { PasswordLessCard } from '@/lib/models';


const CARDS_SEED: PasswordLessCard[] = [{
  name: "Test",
  rfid: "112233"
}];

export enum DongleState {
  Unknown,
  CardReader,
  Master
}

// @ts-ignore
export const PortContext = React.createContext<[string | null, React.Dispatch<React.SetStateAction<string | null>>]>(null);
// @ts-ignore
export const CardsContext = React.createContext<[PasswordLessCard[], React.Dispatch<React.SetStateAction<PasswordLessCard[]>>]>(null);
// @ts-ignore
export const DongleStateContext = React.createContext<[DongleState, React.Dispatch<React.SetStateAction<DongleState>>]>(null);

const initialDongleState = DongleState.Master;

export default function App({ Component, pageProps }: AppProps) {
  const portState = useState<string | null>(null);
  const binaryState = useState<DongleState>(initialDongleState);
  const cardsState = useState<PasswordLessCard[]>(CARDS_SEED);
  const router = useRouter();

  useEffect(() => {
    // if (portState[0] == null) router.push("/ports");
  }, [])

  return (
    // <SafeHydrate> Doesnt work lol
    <GlobalToastProvider>
      <GlobalErrorProvider>
        <PortContext.Provider value={portState}>
          <CardsContext.Provider value={cardsState}>
              <DongleStateContext.Provider value={binaryState}>
                <Head>
                  <script src="http://localhost:8097"></script>
                </Head>
                <Component {...pageProps} />
              </DongleStateContext.Provider>
          </CardsContext.Provider>
        </PortContext.Provider>
      </GlobalErrorProvider>
    </GlobalToastProvider>
  )
}