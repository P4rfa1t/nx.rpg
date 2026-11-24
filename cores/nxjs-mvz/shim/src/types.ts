export type MvzEngineKind = 'rmmv' | 'rmmz';
export type CpuProfile = 'stock_1020' | 'boost_1581' | 'boost_1785';
export type ScalingMode = 'fit' | 'stretch' | 'native';

export interface LaunchManifest {
  schemaVersion: 1;
  sessionId: string;
  returnTo: string;
  logPath: string;
  game: {
    id: string;
    title: string;
    engine: MvzEngineKind;
    core: 'nxjs-mvz';
    gamePath: string;
    contentRoot: string;
    savePath: string;
    execName: string;
  };
  assets: {
    rtpPath: string;
    fallbackFontPath: string;
  };
  runtime: {
    encoding: string;
    rgssVersion: 0;
    cpuProfile: CpuProfile;
    enableTouch: boolean;
    enableSoftwareKeyboard: boolean;
  };
  video: {
    scalingMode: ScalingMode;
    integerScale: boolean;
    vsync: boolean;
    showFps: boolean;
  };
  input: {
    profile: string;
    swapConfirmCancel: boolean;
    exitCombo: string[];
  };
  coreOptions?: {
    mvz?: {
      renderer?: 'auto' | 'webgl2' | 'canvas2d';
      pretendNwjs?: boolean;
      maxTextureSize?: 0 | 2048 | 4096;
    };
  };
}
