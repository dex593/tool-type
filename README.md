# ToolType

ToolType là ứng dụng Win32 C++ nhỏ để nạp danh sách dòng từ `.txt`, `.docx` hoặc Google Docs công khai, sau đó dùng phím dán đã chọn để paste từng dòng vào cửa sổ đang gõ và tự chuyển sang dòng kế tiếp.

## Tính năng chính

- Nạp text từ file `.txt`, `.docx` hoặc link Google Docs công khai.
- Hiển thị đầy đủ dòng trống trong tool, nhưng tự bỏ qua dòng trống khi paste.
- Tự bỏ ký tự đánh dấu đầu dòng `*`, `>`, `-` khi paste.
- Chuẩn hóa khoảng trắng và dấu câu trước khi paste.
- Tùy chọn phím paste bất kỳ; mặc định hỗ trợ Tab/F4.
- On/Off, Pin, Save/Open vị trí đang dùng.
- Nút `Pts` trong cột mở rộng để backup/restore Photoshop settings và font bằng file `.afang`.
- Tooltip hiện full câu khi rê chuột lên từng dòng.
- Tự kiểm tra bản mới qua `check.ini` công khai khi mở tool.
- Giao diện Win32 nhỏ gọn, nền đen trong suốt, có icon và metadata exe.

## Backup/restore Photoshop và font

Mở `Expand` rồi chọn `Pts`. Popup hỗ trợ:

- `Backup PS Settings`: backup settings của đúng một Photoshop version đang cài.
- `Backup Fonts`: backup font người dùng và font hệ thống không thuộc nhóm font Windows mặc định.
- `Backup Settings + Fonts`: lưu cả hai nhóm vào một file `.afang`.
- `Restore`: restore font cho user hiện tại và có thể map settings sang Photoshop version khác, gồm alias/workspace tương thích CS6.

Font được restore vào `%LOCALAPPDATA%\Microsoft\Windows\Fonts`, không cần quyền admin. Nếu file font giống hệt đã tồn tại (kể cả dưới tên `-restored-N`), ToolType dùng lại file đó thay vì ghi thêm một bản trùng, giúp giảm I/O ở các lần restore sau. Khi restore settings, hãy đóng Photoshop để tránh Photoshop ghi đè lại preferences/layout lúc thoát.

Nếu máy có nhiều Photoshop version, backup settings/backup kết hợp sẽ yêu cầu chọn đúng một version đang cài. Khi restore, ToolType cho chọn version nguồn trong `.afang` (nếu archive có nhiều version) và version Photoshop đích đang có trên máy; nguồn và đích có thể khác nhau.

Backup và restore chạy trên worker riêng nên popup vẫn có thể repaint/di chuyển trong lúc đọc, giải nén, ghi hoặc đăng ký font. Khi thao tác đang chạy, nút `Đóng` đổi thành `Hủy`: backup bị hủy sẽ xóa file `.afang` chưa hoàn tất; restore bị hủy sẽ dừng trước bước/file tiếp theo và giữ lại những file đã restore xong trước đó.

## Build local

Cần MinGW-w64/MSYS2 có `g++.exe`, `windres.exe` và zlib. Script sẽ tự tìm compiler trong PATH hoặc các đường dẫn phổ biến:

- `C:\Users\lsc\mingw32\bin`
- `C:\msys64\mingw64\bin`
- `C:\msys64\mingw32\bin`

Chạy:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

Kết quả: `ToolType.exe`.

Chạy test:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1 -Coverage
```

## Build trên GitHub

Repo có GitHub Actions tại `.github/workflows/build.yml`.

Mỗi lần push/pull request hoặc chạy thủ công bằng `workflow_dispatch`, GitHub sẽ:

1. Checkout source.
2. Cài MSYS2 MinGW-w64.
3. Chạy regression tests và coverage gate.
4. Chạy `build.ps1`.
5. Đóng gói `ToolType.exe` vào `ToolType.zip` và upload artifact ZIP.
6. Với push lên `master`/`main` hoặc chạy thủ công, tạo/cập nhật GitHub Release `v<version>` với release notes gọn và asset `ToolType.zip`.

## Cập nhật phiên bản

ToolType đang dùng version `1.0.0.0` trong code/resource. File `check.ini` ở repo chứa:

```ini
[update]
version=1.0.0.0
download=https://github.com/dex593/tool-type/releases/latest/download/ToolType.zip
```

Khi mở app, ToolType tải `https://github.com/dex593/tool-type/raw/refs/heads/master/check.ini`.
Nếu `version` trong file này cao hơn version trong code, status sẽ hiện:
`Đã có phiên bản mới, click để tải ngay.` Người dùng bấm vào dòng status để mở link `download`.
Nếu chưa có bản mới hoặc không tải được file kiểm tra, status sẽ là:
`Bạn đang sử dụng phiên bản mới nhất.`

Khi phát hành bản mới, hãy bump đồng bộ:

- `kToolVersion` trong `main.cpp`;
- `FILEVERSION`, `PRODUCTVERSION`, `FileVersion`, `ProductVersion` trong `resource.rc`;
- `version=` trong `check.ini`.

## Lưu ý bảo mật/antivirus

ToolType có dùng low-level keyboard hook để bắt phím paste và thao tác clipboard theo yêu cầu người dùng. Đây là hành vi hợp lệ của app, nhưng có thể bị một số antivirus ML/heuristic đánh dấu nhầm. Bản build đã có:

- manifest `asInvoker`, không yêu cầu admin;
- version info/metadata rõ ràng;
- DEP/NX và ASLR linker flags;
- hook chỉ cài khi app đang On và đã có text để paste.
