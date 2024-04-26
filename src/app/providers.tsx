'use client'

import GlobalErrorProvider from "@/components/GlobalErrorProvider";
import GlobalToastProvider from "@/components/GlobalToastProvider";
import { PasswordLessCard } from "@/lib/models";
import React from "react";
import { useState } from "react";


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


export default function Providers({
    children, // will be a page or nested layout
}: {
    children: React.ReactNode
}) {

    const portState = useState<string | null>(null);
    const binaryState = useState<DongleState>(initialDongleState);
    const cardsState = useState<PasswordLessCard[]>([]);
    return (

        <GlobalToastProvider>
            <GlobalErrorProvider>
                <PortContext.Provider value={portState}>
                    <CardsContext.Provider value={cardsState}>
                        <DongleStateContext.Provider value={binaryState}>

                            {children}

                        </DongleStateContext.Provider>
                    </CardsContext.Provider>
                </PortContext.Provider>
            </GlobalErrorProvider>
        </GlobalToastProvider >
    )
}

