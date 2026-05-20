# ToolType

ToolType là ứng dụng Win32 C++ nhỏ để nạp danh sách dòng từ `.txt`, `.docx` hoặc Google Docs công khai, sau đó dùng phím dán đã chọn để paste từng dòng vào cửa sổ đang gõ và tự chuyển sang dòng kế tiếp.

## Tính năng chính

- Nạp text từ file `.txt`, `.docx` hoặc link Google Docs công khai.
- Hiển thị đầy đủ dòng trống trong tool, nhưng tự bỏ qua dòng trống khi paste.
- Tự bỏ ký tự đánh dấu đầu dòng `*`, `>`, `-` khi paste.
- Chuẩn hóa khoảng trắng và dấu câu trước khi paste.
- Tùy chọn phím paste bất kỳ; mặc định hỗ trợ Tab/F4.
- On/Off, Pin, Save/Open vị trí đang dùng.
- Tooltip hiện full câu khi rê chuột lên từng dòng.
- Giao diện Win32 nhỏ gọn, nền đen trong suốt, có icon và metadata exe.

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

## Build trên GitHub

Repo có GitHub Actions tại `.github/workflows/build.yml`.

Mỗi lần push/pull request hoặc chạy thủ công bằng `workflow_dispatch`, GitHub sẽ:

1. Checkout source.
2. Cài MSYS2 MinGW-w64.
3. Chạy `build.ps1`.
4. Upload artifact `ToolType.exe`.

## Lưu ý bảo mật/antivirus

ToolType có dùng low-level keyboard hook để bắt phím paste và thao tác clipboard theo yêu cầu người dùng. Đây là hành vi hợp lệ của app, nhưng có thể bị một số antivirus ML/heuristic đánh dấu nhầm. Bản build đã có:

- manifest `asInvoker`, không yêu cầu admin;
- version info/metadata rõ ràng;
- DEP/NX và ASLR linker flags;
- hook chỉ cài khi app đang On và đã có text để paste.
