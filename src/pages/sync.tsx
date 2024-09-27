import CommandTerminal from '@/components/CommandTerminal';
import { Command } from '@tauri-apps/api/shell';
import { useRouter } from 'next/navigation';
import { useContext, useEffect, useRef, useState } from 'react';
import { CardsContext, NewCardsContext, PortContext } from './_app';
import { getEspBinDir } from '@/lib/services';
import { useToast } from '@/hooks/useToast';

export default function SyncView() {
  const [isLoading, setisLoading] = useState<boolean>(false);
  const syncCommand = useRef<Command | null>(null);
  const router = useRouter();
  const setToast = useToast();
  const [newCards, setNewCards] = useContext(NewCardsContext);
  const [cards, setCards] = useContext(CardsContext);
  const [selectedPort, setSelectedPort] = useContext(PortContext);

  const runCommands = async () => {
    const invoke = (await import('@tauri-apps/api')).invoke;
    const Command = (await import('@tauri-apps/api/shell')).Command;
    const path = (await import('@tauri-apps/api')).path;

    let location = await invoke('save_cards_to_csv_command', { cards: newCards });
    let nvsBinDir = await path.join(await path.appLocalDataDir(), `save_data.bin`);

    //nvs_gen, the 0x5000 is the binary offset of the nvs partition and can be seen using parttool get_partition_info

    syncCommand.current = Command.sidecar("bin/dist/nvs_gen", [`generate`, `${location}`, `${nvsBinDir}`, `0x5000`]);
    setisLoading(true);
    const nvs_res = await syncCommand.current.execute();
    setisLoading(false);
    console.log({ nvs_res });

    const espBin = await getEspBinDir();
    //parttool
    syncCommand.current = Command.sidecar("bin/dist/parttool", [ `-e`, `${espBin}`, `--port`, `${selectedPort}`, `--baud`, `115200`, `write_partition`, `--partition-name=nvs`, `--input`, `${nvsBinDir}`]);
    setisLoading(true);
    const part_res = await syncCommand.current.execute();
    setisLoading(false);
    console.log({part_res});
    setCards(newCards);
    setToast("Finished syncing! Please load key binary to use the device now")


    // Dont go back to ports so that user can still do stuff like load main binary on a granular level
    
    // // TODO: abstract to hook
    // setNewCards([]);
    // setCards([]);
    // router.push('/ports');
    router.push('/');

  }

  useEffect(() => {
    // getDataCommand.current.on('close', () => {
    // 	console.log(`Saved BinaryFile in: ${readFileBin}`);
    // 	setToast(`Saved BinaryFile in: ${readFileBin}`);
    // 	setRunningCommand(false);
    // 	router.push("/");
    // })
    runCommands();


    // CAll to rust save csv
    //Turn off rust card reader server

    // load main binary


  }, [])

  return (
    <>
      <button
        onClick={() => router.push("/")}
        className="text-gray text-left p-3 bg-[#213352] w-full text-[white]">Back
      </button>

      <div className="flex flex-col items-center bg-[#292828] h-full w-full justify-center">
        <div className="pt-24 justify-center text-4xl pb-6 items-center text-center rounded-lg">
          <p><strong className='animate-pulse select-none text-center text-green-700'>Saving cards to ESP32!</strong></p>
          <CommandTerminal enabled={isLoading} className="p-6 flex w-auto text-left" commandObj={syncCommand} />
        </div>
      </div>
    </>
  )
}