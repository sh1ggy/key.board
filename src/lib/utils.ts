
export function arraysEqual<Type>(a: Type[], b: Type[]) {
  if (a === b) return true;
  if (a == null || b == null) return false;
  if (a.length !== b.length) return false;

  for (var i = 0; i < a.length; ++i) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

export const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export function truncateString(str: string, charsToUse: number = 75) {
  if (str.length <= charsToUse) {
    return str;
  } else {
    const truncated = str.substr(0, charsToUse) + '...' + str.substr(-charsToUse);
    return truncated;
  }
}

// TODO: use
export enum KeyDotErrors {
    CouldNotReadPort,
    CouldNotWritePort,
    CouldNotReadPortBytes,
}