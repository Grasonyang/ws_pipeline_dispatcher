# 貢獻 stream-data-pipeline

感謝您對 `stream-data-pipeline` 的貢獻興趣！本文件引導貢獻者了解我們的開發流程、期望和最佳實踐。

## 專案理念

在開始貢獻前，請理解 `stream-data-pipeline` 遵循的核心 UNIX 原則：

- **單一責任原則**：每個 applet（`stream_merge`、`log_parse`、`clip_store`）只有一個責任
- **組合優於複雜性**：工具設計用於通過管道組合，而非單體架構
- **流資料紀律**：stdout 僅傳輸結構化資料；stderr 僅寫診斷訊息
- **最小依賴**：使用 C 語言實作，適合資源受限的嵌入式環境

貢獻時，請遵守此理念。避免為 applet 添加過度功能或打破 UNIX 哲學。

---

## 1. 我們需要什麼樣的幫忙？

我們歡迎以下領域的貢獻：

### 新功能與函數開發
- 當邊界設備（ESP32）或數據源支持新功能時，開發新的函數
- 擴展 `stream_merge`、`log_parse` 或 `clip_store` 的功能
- 增加對新數據格式或協議的支持

### 提供文件與範例程式碼
- 改進和更新 README 和文件
- 提供範例程式碼和使用模式
- 修正錯字或澄清不清楚的部分
- 擴展 `.docs/` 目錄的架構說明

### 錯誤回報與修正
- 報告您在使用工具時遇到的問題
- 修正現有代碼中的錯誤
- 改進錯誤處理和邊界情況覆蓋
- 修正文件中的錯誤和不一致之處

### 性能與最佳化
- 對關鍵路徑進行性能分析和最佳化
- 減少嵌入式環境的內存佔用
- 改進流資料處理性能

### 測試改進
- 添加更全面的單元測試
- 編寫複雜場景的整合測試
- 在不同平台和邊界情況下測試

### 尋找可以貢獻的 Issues

我們使用 GitHub 標籤（Labels）幫助貢獻者找到合適的工作。**初次貢獻者應從這些標籤開始**：

- **`good first issue`** ⭐ - 適合新貢獻者的好起點，通常工作量小、難度低
- **`help wanted`** - 特別需要幫助的領域，優先級高
- **`bug`** - 已知需要修復的問題
- **`documentation`** - 需要改進的文件，無需深度技術知識
- **`enhancement`** - 功能請求和改進
- **`question`** - 設計問題和討論

**如何找到 Issues：**
```bash
# 在 GitHub Issues 頁面篩選標籤
1. 點擊「Issues」標籤
2. 左側篩選器選擇「Labels」
3. 選擇 "good first issue" 或 "help wanted"
4. 閱讀 issue 描述，在評論中提問
```

如果不確定某個 issue，可以在評論中提問！維護者很樂意指導新貢獻者。

---

## 2. 開發規範與程式寫法

為了保持代碼的一致性和可維護性，請遵循以下規範。

### 開發環境設置

#### 前置需求

- **C 編譯器**：GCC 或 Clang（C11 或更新版本）
- **POSIX 環境**：Linux、macOS 或 WSL2
- **構建工具**：GNU Make、標準 POSIX 工具（sh、grep、tail）
- **選配**：`valgrind` 用於內存洩漏檢測、`clang-format` 用於代碼格式化

#### 編譯與構建

```bash
# 克隆倉庫
git clone <repo-url>
cd stream-data-pipeline

# 編譯所有 applet
make

# 使用調試符號編譯，無最佳化（建議開發時使用）
make clean
CFLAGS="-g -O0 -Wall -Wextra" make

# 編譯並執行測試
make test

# 執行端對端煙霧測試
make smoke
```

構建系統會輸出二進制檔案至 `build/`。

### C 代碼風格

#### 1. 縮進與格式化
- 使用 4 個空格縮進（不是製表符）
- 行長度：目標 100 個字符，硬性限制 120
- 如需自動格式化，使用 `clang-format`：
  ```bash
  clang-format -i applets/*.c lib/*.c
  ```
  或使用 `astyle`：
  ```bash
  astyle --style=bsd --indent=spaces=4 --pad-oper --pad-header applets/*.c lib/*.c
  ```

#### 2. 命名規則
- 函數和變量使用 `snake_case`：`read_metadata_sidecar`、`buffer_size`
- 常數和宏使用 `SCREAMING_SNAKE_CASE`：`MAX_BUFFER_SIZE`、`SENTINEL_MARKER`
- 內部靜態函數以 `_` 開頭：`_parse_metadata`、`_validate_record`
- 使用有意義的名稱；避免縮寫，除非通用（例：`ts_ms` 表示時間戳毫秒）

#### 3. 函數設計
- 函數長度控制在 100 行以內為佳
- 用註解記錄公開函數（放在函數上方）：
  ```c
  // 從 sidecar 檔案讀取元數據並填充緩衝區。
  // 成功時回傳有效記錄數，I/O 錯誤時回傳 -1。
  int read_metadata_sidecar(const char *path, struct metadata_buf *buf);
  ```
- 對文件本地函數使用 `static`

#### 4. 錯誤處理
- 檢查**所有**系統調用的返回值，不要假設成功
- 對錯誤使用 perror 風格的日誌輸出到 stderr
- 使用有意義的退出代碼：
  - `0` 成功
  - `1` 一般錯誤
  - `2` 使用錯誤（參數錯誤）
  - 其他代碼表示 applet 特定故障

- 範例：
  ```c
  if (read(fd, buf, n) < 0) {
      fprintf(stderr, "error: read failed on %s: %s\n", filename, strerror(errno));
      return -1;
  }
  ```

#### 5. 內存管理
- 從函數返回前釋放所有分配的內存
- 必要時使用 `calloc()` 進行零初始化結構
- **強制要求**：使用 valgrind 測試所有代碼，檢查內存洩漏：
  ```bash
  valgrind --leak-check=full ./build/applet_name <args>
  ```

### 頭文件（Header）規範

- 將 `.h` 檔案放在 `lib/` 目錄中
- 包含保護：
  ```c
  #ifndef LIB_FOO_H
  #define LIB_FOO_H
  // ...
  #endif
  ```
- 記錄公開 API
- 避免 `#include` 循環；必要時使用前向聲明

### 殼層腳本規範（測試）

- Shebang：`#!/bin/sh`（POSIX，非 bash）
- 安全設置：`set -eu`（未設定變量時錯誤，首次錯誤時退出）
- 陷阱清理：`trap 'rm -rf "$TMP_DIR"' EXIT`
- 本地變量使用小寫，環境變量使用大寫
- 引用所有變量擴展：`"$var"`，不是 `$var`

## 流資料紀律與日誌（CRITICAL）

這是我們遵循的 UNIX 哲學的**關鍵**部分，**必須遵守**：

### stdout - 僅用於資料輸出
- 專用於結構化輸出（JSON Lines 格式）
- **絕對不允許**進度訊息、調試信息或日誌
- 每行一筆記錄，格式一致
- 例子（來自 `stream_merge`）：
  ```json
  {"type":"clip","session_id":"sess_1","complete":true,"offset":0,"length":128,"ts":1000}
  ```

### stderr - 僅用於診斷
- 專用於診斷日誌、警告和錯誤
- 使用 `stream_logger` 巨集：`LOG_WARN(...)`、`LOG_ERROR(...)`
- 包含上下文（例：檔案名、記錄號、操作類型）
- 例子：
  ```c
  LOG_WARN("skipping invalid JSON line %d: %s", line_num, line);
  LOG_ERROR("failed to open sidecar file: %s", strerror(errno));
  ```

## 測試

### 執行測試

```bash
# 執行所有單元和整合測試
make test

# 僅執行特定 applet 測試（替換 APP）
tests/test_APP.sh

# 執行端對端煙霧測試
make smoke

# 檢查內存洩漏（必須做）
valgrind --leak-check=full ./build/stream_merge <args>
valgrind --leak-check=full ./build/log_parse <args>
valgrind --leak-check=full ./build/clip_store <args>
```

### 編寫測試

**殼層整合測試** (`tests/test_*.sh`)：

- 每個主要 applet 或元件一個測試檔案
- 使用 `set -eu` 確保安全
- 用 `mktemp -d` 建立臨時目錄，用 trap 清理
- 使用助手函數：
  ```sh
  check_eq "測試名稱" "預期" "實際"
  check_contains "測試名稱" "子字符串" "搜尋對象"
  ```
- 測試正常路徑、**錯誤情況**和**邊界情況**
- 範例：
  ```bash
  #!/bin/sh
  set -eu
  TMP_DIR=$(mktemp -d)
  trap 'rm -rf "$TMP_DIR"' EXIT

  check_eq() {
      name=$1; expected=$2; actual=$3
      if [ "$expected" != "$actual" ]; then
          printf 'FAIL %s\nexpected: %s\nactual:   %s\n' "$name" "$expected" "$actual" >&2; exit 1
      fi
  }

  check_contains() {
      name=$1; needle=$2; haystack=$3
      case "$haystack" in
          *"$needle"*) ;;
          *) printf 'FAIL %s\nneedle: %s\nhaystack: %s\n' "$name" "$needle" "$haystack" >&2; exit 1 ;;
      esac
  }

  # 正常測試案例
  echo '{"id":1,"msg":"test"}' | ./build/log_parse --filter id=1 > "$TMP_DIR/out"
  check_eq "filter id=1" '{"id":1,"msg":"test"}' "$(cat $TMP_DIR/out)"

  # 錯誤情況測試
  echo 'malformed' | ./build/log_parse --filter id=1 2>"$TMP_DIR/err"
  check_contains "error message" "error" "$(cat $TMP_DIR/err)"
  ```

**C 單元測試**：

- 使用 `-g` 編譯並在 `valgrind` 下運行
- 獨立測試 `lib/` 中的助手函數
- 範例：
  ```c
  void test_buffer_append() {
      struct buffer buf = {0};
      buffer_append(&buf, "test", 4);
      assert(buf.len == 4);
      free(buf.data);
  }
  ```

### 測試覆蓋率目標

- 核心邏輯（解析、過濾、儲存）應有 >80% 的測試覆蓋率
- 錯誤路徑（I/O 故障、格式錯誤的輸入）應明確測試
- 並發寫入情況中的競賽條件應被驗證

### Git 提交訊息格式

所有提交都應遵循此**標準化格式**：

```
[action]: [description]
```

**行動類型：**
- `init` - 專案初始化
- `feat` - 新功能或 applet
- `fix` - 錯誤修復
- `docs` - 僅文件更改
- `style` - 代碼格式化（無邏輯更改）
- `refactor` - 代碼重構（無功能更改）
- `test` - 測試添加或修復
- `chore` - 構建系統、工具、依賴

**範例：**
```
[feat]: add regex parsing to log_parse
[fix]: handle EOF in stream_merge sidecar drain
[docs]: update API documentation in .docs/
[test]: add continuity break test for stream_merge
[chore]: update Makefile to include valgrind targets
```

### Issue 與 Pull Request 模板

建立 issue 或 pull request 時，請使用提供的模板。它們幫助我們理解：
- **Issue 模板**：錯誤複現步驟、預期與實際行為、環境細節
- **Pull Request 模板**：您做了什麼更改、為什麼、做了什麼測試

---

## 3. 實際開發流程

本節完整說明從開始到完成的流程。遵循此流程確保貢獻流暢高效。

### 第 1 步：Fork 倉庫

首先，為自己建立倉庫的副本：

1. 前往 [stream-data-pipeline GitHub 倉庫](https://github.com/Grasonyang/stream-data-pipeline)
2. 點擊右上角的 **"Fork"** 按鈕
3. 這會在您的 GitHub 帳戶下建立一個副本（例：`your-username/stream-data-pipeline`）

**為什麼 Fork？** Fork 讓您在自己的倉庫中安全地工作，無需主倉庫的權限，也不會影響主倉庫的開發。

### 第 2 步：克隆 Fork 到本地

克隆 fork 的倉庫到您的計算機，並設置 upstream 以保持與主倉庫同步：

```bash
# 克隆您的 fork（不是原始倉庫）
git clone https://github.com/your-username/stream-data-pipeline.git
cd stream-data-pipeline

# 將原始倉庫添加為 "upstream" 以進行同步
git remote add upstream https://github.com/Grasonyang/stream-data-pipeline.git

# 驗證您有兩個遠端
git remote -v
# origin    https://github.com/your-username/stream-data-pipeline.git (fetch)
# origin    https://github.com/your-username/stream-data-pipeline.git (push)
# upstream  https://github.com/Grasonyang/stream-data-pipeline.git (fetch)
# upstream  https://github.com/Grasonyang/stream-data-pipeline.git (push)
```

### 第 3 步：建立功能分支

為您的工作建立一個分支。使用有描述性的名稱：

```bash
# 先更新 main 到最新
git fetch upstream
git checkout main
git merge upstream/main

# 建立您的功能/修復分支
git checkout -b feat/my-new-feature
# 或針對錯誤修復：
git checkout -b fix/issue-123
```

**分支命名規則：**
- `feat/` - 新功能或 applet
- `fix/` - 錯誤修復
- `docs/` - 文件更新
- `test/` - 測試改進
- `refactor/` - 代碼重構

### 第 4 步：開發與測試

開發您的功能並在本地進行**完整測試**：

```bash
# 進行您的更改
nano applets/stream_merge.c

# 構建（使用調試標誌）
make clean
CFLAGS="-g -O0 -Wall -Wextra" make

# 執行所有測試（必須通過）
make test
make smoke

# 檢查內存洩漏（必須執行）
valgrind --leak-check=full ./build/stream_merge ...
valgrind --leak-check=full ./build/log_parse ...
valgrind --leak-check=full ./build/clip_store ...

# 驗證代碼風格
clang-format --dry-run -i applets/*.c lib/*.c
```

### 第 5 步：提交您的更改

遵循我們的提交格式提交具有描述性的訊息：

```bash
# 查看您做了哪些更改
git status
git diff

# 暫存更改
git add applets/stream_merge.c

# 按照 [action]: [description] 格式提交
git commit -m "[feat]: add window-size parameter to stream_merge"

# 查看您的提交
git log --oneline -3
```

**提交訊息撰寫建議：**
- 使用命令式語氣：「add」而非「added」或「adds」
- 第一行不超過 50 字符
- 如需更多說明，空一行後詳細描述
- 引用相關 issue：「fixes #123」

### 第 6 步：推送到您的 Fork

推送您的分支到 GitHub fork：

```bash
git push origin feat/my-new-feature
```

### 第 7 步：建立 Pull Request

1. 前往 [原始倉庫](https://github.com/Grasonyang/stream-data-pipeline)
2. 您將看到從您的 fork 建立 Pull Request 的提示
3. 點擊 **"Compare & pull request"**
4. 填寫 PR 模板：
   - **標題**：清晰的更改摘要（例：「Add regex support to log_parse」）
   - **描述**：做了什麼和為什麼（使用模板提供的結構）
   - **測試**：描述您執行的測試（`make test`、`make smoke`、valgrind 等）
   - **相關 Issues**：用 `fixes #123` 或 `closes #456` 參考任何 issues

5. 點擊 **"Create pull request"**

### 第 8 步：應對代碼審查

維護者將審查您的 PR：

- 及時回應反饋（通常 24-48 小時內）
- 在同一分支上進行要求的更改
- 提交新的 commit 而非強制覆蓋（maintainer 會在合併前 squash）
- 推送新提交（PR 將自動更新，無需關閉和重新開啟）

```bash
# 根據審查反饋進行更改
nano applets/stream_merge.c

# 測試您的修改
make test && make smoke
valgrind --leak-check=full ./build/stream_merge ...

# 提交並推送
git add applets/stream_merge.c
git commit -m "[fix]: address review feedback on error handling"
git push origin feat/my-new-feature
```

### 第 9 步：合併與清理

一旦批准且所有測試通過：

1. Maintainer 會將您的 PR 合併到 `main`
2. 刪除遠端分支：
   ```bash
   git push origin --delete feat/my-new-feature
   ```
3. 刪除本地分支：
   ```bash
   git branch -d feat/my-new-feature
   ```
4. 更新您的本地 main：
   ```bash
   git fetch upstream
   git checkout main
   git merge upstream/main
   ```

### 與 Upstream 同步（保持 Fork 最新）

如果您的 PR 需要時間或想保持與主倉庫同步：

```bash
# 從 upstream 獲取最新更新
git fetch upstream

# 將您的分支重新基於最新的 upstream/main
git rebase upstream/main feat/my-new-feature

# 強制推送您更新的分支（僅推到您自己的 fork！）
git push origin --force-with-lease feat/my-new-feature
```

---

## 性能考量

- **流資料處理**：Applet 應以恆定內存處理流資料（不緩衝整個輸入）
- **文件 I/O**：文件操作應使用帶緩衝的 I/O；避免逐位元組讀寫
- **大文件**：對大型 `.bin` 檔案使用 `mmap()` 或順序 I/O，盡可能避免隨機存取
- **性能分析**：如添加新的關鍵路徑，使用 `perf record` 進行性能分析：
  ```bash
  perf record -g ./build/stream_merge ...
  perf report
  ```

---

## 故障排除

### 我的 PR 有衝突
```bash
# 獲取並在最新版本上重新基於
git fetch upstream
git rebase upstream/main
# 在您的編輯器中解決衝突（搜尋 <<< === >>>）
git add .
git rebase --continue
git push origin --force-with-lease feat/my-new-feature
```

### 我需要撤銷我的上一次提交
```bash
git reset --soft HEAD~1  # 撤銷提交，保留更改（可重新提交）
git reset --hard HEAD~1  # 撤銷提交並丟棄更改（謹慎使用）
```

### 我想將 fork 更新到最新版本
```bash
git fetch upstream
git checkout main
git merge upstream/main
git push origin main
```

### 我不小心提交到了 main，而不是功能分支
```bash
# 取消提交，保留更改
git reset HEAD~1

# 建立新分支，提交會跟著您
git checkout -b fix/my-fix

# 回到 main，與 upstream 同步
git checkout main
git reset --hard upstream/main
```

---

## 代碼審查期望

當您的代碼被審查時，我們會關注以下方面：

- **正確性**：它是否按預期工作？是否有邏輯錯誤？
- **風格**：它是否遵循我們的代碼風格規範？
- **測試**：測試是否充分？是否涵蓋邊界情況？
- **文件**：代碼是否有適當的註解？是否更新了 README 或 .docs/?
- **性能**：可以更高效嗎？是否引入了不必要的內存消耗？
- **UNIX 哲學**：是否遵循 UNIX 原則？是否保持了流資料紀律？

**審查不是批評** - 我們的目標是幫助您寫出更好的代碼，並確保項目的質量。

---

## 文件與文檔

### 代碼註解

- 註解*為什麼*，不是*是什麼*
- 解釋非顯而易見的設計決策
- 註解應簡潔但有意義
- 例：
  ```c
  // 連續性檢查：如果序列號跳躍 > 1，則發出部分 clip。
  // （邊界設備可能丟棄封包；我們不重建，我們重新開始。）
  if (expected_seq != actual_seq) {
      emit_partial_clip();
  }
  ```

### README 與文件

- 保持 `README.md` 高層次和面向用戶
- 設計決策放在 `.docs/`
- 合約規範（例：JSON 模式、CLI 參數）放在 `.docs/core/contract.md`
- 各 applet 實作細節放在 `.docs/applets/`
- 添加需求時更新合規矩陣（`.docs/core/compliance.md`）

---

## 報告問題

報告錯誤時，請提供：

- **複現步驟**：如何重現問題（越詳細越好）
- **預期行為**：應該發生什麼
- **實際行為**：實際發生了什麼
- **輸入數據**：導致問題的最小範例輸入
- **錯誤訊息**：完整的 stderr 輸出
- **環境**：
  - 作業系統（Linux 版本、macOS 版本等）
  - 編譯器版本（`gcc --version`）
  - 構建標誌（您用什麼 CFLAGS 編譯的）

**範例 Issue 標題：**
```
[BUG] stream_merge crashes on empty metadata file
[BUG] log_parse --filter type=data skips valid records
```

---

## 有問題？

- **設計問題？** 開立標籤為 `question` 的 issue，詳細說明您的想法
- **卡住了？** 在 PR 或 issue 評論中提問。我們很樂意幫助！
- **安全問題？** 🔒 私下電郵維護者，不要開立公開 issue
- **一般反饋？** 在 GitHub Discussions 中開啟討論

---

## 開發者檢查清單

在提交 PR 前，請確認：

- [ ] 代碼遵循風格指南（使用 `clang-format`）
- [ ] 所有測試通過：`make test && make smoke`
- [ ] 無內存洩漏：`valgrind --leak-check=full`
- [ ] 維護 stdout/stderr 紀律（無日誌在 stdout）
- [ ] 文件已更新（README、.docs/、代碼註解）
- [ ] 提交訊息遵循 `[action]: [description]` 格式
- [ ] PR 填寫了完整的模板
- [ ] 引用了相關 issues（如果有）

---

## 致謝

感謝您對 `stream-data-pipeline` 的貢獻！無論是代碼、文件還是錯誤報告，您的幫助都讓這個專案更好。

**所有貢獻者都將在項目的 CONTRIBUTORS.md 中被認可。**

---

*最後更新：2026 年*
