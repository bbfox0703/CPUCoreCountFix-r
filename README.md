fork of https://github.com/alexstrout/CPUCoreCountFix


## 🛠️ CPUCoreCountFix 修改說明

本專案為對 [`alexstrout/CPUCoreCountFix`](https://github.com/alexstrout/CPUCoreCountFix) 的改良版本，進行以下修正與優化以提高相容性、安全性與可維護性。
  
主要是針對舊遊戲、如 刀劍神域-虛空幻界- 會因CPU大於或是等於32核造成無法執行，以刀劍神域-虛空幻界-來說，將x64版本的dinput8.dll放在遊戲執行檔目錄即可。

---

### ✅ 主要修改內容

| 類別              | 修改內容                                              | 說明                                                                                                  |
| --------------- | ------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| 🐛 Bug Fix      | 修正 `GetLogicalProcessorInformationDetour` 的長度裁切邏輯 | 原本錯誤將核心數與 `ReturnedLength`（bytes）混為一起，已改為依 `sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)` 計算正確裁切大小。 |
| 🔒 錯誤處理         | 移除 `RaiseException(EXCEPTION_NONCONTINUABLE)`     | 改為使用 `SetLastError` 搭配回傳 `E_FAIL`，避免因 DLL 載入錯誤直接導致Crash。                                           |
| 💾 記憶體管理        | 加入 `FreeLibrary(moduleHandle)` 清理                 | 修正 `LoadLibrary` 所載入之原始 `DINPUT8.dll` 在 DLL 卸載時未釋放的資源洩漏問題。                                          |
| 🔁 Detours Hook | 新增 `DetourDetach`                                 | 在 `DLL_PROCESS_DETACH` 階段解除掛鉤，防止殘留 hook 導致未定行為或遊戲關閉時Crash。                                             |
| 🔄 結構調整         | 分離 `Init()` 與 `Cleanup()`                         | 提高初始化與資源釋放流程的可讀性與可維護性。                                                                              |

---

### 📌 兼容性說明

* 本 DLL 預期用於取代 `dinput8.dll`，繼承其接口，並修正多核心 CPU 對某些舊遊戲（如 Sword Art Online: Hollow Realization Deluxe Edition / 刀劍神域-虛空幻界- 豪華版 ）造成的崩潰問題。
* 若你的系統為非標準 Windows 安裝路徑，請手動修正 `LoadLibrary` 中的 `DINPUT8.dll` 絕對路徑。

