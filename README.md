fork of https://github.com/alexstrout/CPUCoreCountFix

## 🛠️ CPUCoreCountFix-r 修改說明

此為針對 [`alexstrout/CPUCoreCountFix`](https://github.com/alexstrout/CPUCoreCountFix) 的改良版本，功能是一樣的。
  
主要是針對舊遊戲、如「刀劍神域-虛空幻界-」會因CPU大於或是等於32邏輯核心數 (例如16C32T的5950X、9950X3D、9955HX3D等CPU) 造成無法執行、啟動失敗，以「刀劍神域-虛空幻界-」來說，將x64版本的dinput8.dll放在遊戲執行檔目錄即可。  
要注意的是Windows要裝在預設位置 C:\Windows、因為在DLL中是寫死的。此DLL會回報邏輯核心數為12。 
  
其它遊戲也有限制例如：  
Far Cry 2：31 threads  
Far Cry 3：29 threads  
Far Cry 4：31 threads  
Warhammer 40,000: Space Marine：26 threads  
The Witcher 2: Assassins of Kings Enchanced Edition：31 threads  
Child of Light：16 threads  
  

---

### ✅ 主要修改內容

| 類別              | 修改內容                                              | 說明                                                                                                  |
| --------------- | ------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| 🐛 錯誤修正         | 修正 `GetLogicalProcessorInformationDetour` 的長度裁切邏輯 | 原本將核心數與 `ReturnedLength`（bytes）混為一起，已改為依 `sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)` 計算正確裁切大小。 |
| 🔒 錯誤處理         | 移除 `RaiseException(EXCEPTION_NONCONTINUABLE)`     | 改為使用 `SetLastError` 搭配回傳 `E_FAIL`，避免因 DLL 載入錯誤直接導致Crash。                                           |
| 💾 記憶體管理        | 加入 `FreeLibrary(moduleHandle)` 清理                 | 修正 `LoadLibrary` 所載入之原始 `DINPUT8.dll` 在 DLL 卸載時未釋放的資源洩漏問題。                                          |
| 🔁 Detours Hook | 新增 `DetourDetach`                                 | 在 `DLL_PROCESS_DETACH` 階段解除掛鉤，防止殘留 hook 導致未定行為或遊戲關閉時Crash。                                             |
| 🔄 結構調整         | 分離 `Init()` 與 `Cleanup()`                         | 提高初始化與資源釋放流程的可讀性與可維護性。                                                                              |

---

### 📌 兼容性說明

* 本 DLL 預期用於取代 `dinput8.dll`，並呼叫原DLL，以修正多核心 CPU 對某些舊遊戲（如 Sword Art Online: Hollow Realization Deluxe Edition / 刀劍神域-虛空幻界- 豪華版 ）造成的無法啟動問題。
* 若你的系統為非標準 Windows 安裝路徑，請手動修正 `LoadLibrary` 中的 `DINPUT8.dll` 絕對路徑。自行Compile一份。

