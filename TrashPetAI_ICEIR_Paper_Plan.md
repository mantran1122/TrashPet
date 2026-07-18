# Kế hoạch viết báo cho ICEIR 2026 — TrashPet AI Desktop Assistant

**Hội nghị mục tiêu:** ICEIR 2026 — *International Conference on Intelligent Computing and Efficient Information Retrieval* (iceir.nctu.edu.vn)
**Định dạng theo CFP:** tối đa 14 trang / 6000 từ, tiếng Anh, PDF theo template Springer, nộp qua hệ thống online. → Bản kế hoạch này nhắm **bản rút gọn 8 trang** (short/regular paper cô đọng), vẫn đủ các mục bắt buộc để mở rộng lên full paper nếu cần.
**Tỉ lệ chấp nhận tham khảo:** ~30%.

**Vì sao TrashPet AI phù hợp chủ đề hội nghị:** ICEIR nhấn mạnh *Intelligent Computing* (hệ thống có AI, tác nhân thông minh, ứng dụng thực tế) và *Efficient Information Retrieval / xử lý dữ liệu hiệu quả* trên thiết bị. TrashPet AI là một desktop agent tích hợp LLM (Gemini/GPT/Claude) kết hợp quản lý tài nguyên hệ thống (Recycle Bin) theo thời gian thực — có thể đóng khung thành bài toán "intelligent on-device agent with efficient local resource retrieval/management", đúng giao điểm 2 chủ đề chính.

---

## 1. Đề xuất tiêu đề (chọn 1, có thể tinh chỉnh sau khi viết Abstract)

- "TrashPet AI: A Multi-LLM Desktop Companion Agent with Adaptive Gamified Storage Management"
- "An Intelligent Desktop Pet Agent for Real-Time File System Cleanup via Multi-Provider LLM Integration"
- "Gamifying Recycle Bin Management: A Desktop AI Companion with State-Aware File Retrieval"

→ Ưu tiên phương án 1 (nêu rõ 2 đóng góp: multi-LLM + gamified system management).

## 2. Cấu trúc 8 trang (phân bổ dung lượng)

| # | Mục | Trang | Nội dung chính |
|---|-----|-------|----------------|
| 1 | Title, Authors, Abstract, Keywords | 0.3 | Tóm tắt bài toán, phương pháp, đóng góp, kết quả (150–200 từ) |
| 2 | Introduction | 1.0 | Động lực, vấn đề (quản lý dung lượng ổ đĩa, trải nghiệm desktop assistant khô khan), câu hỏi nghiên cứu, đóng góp (3 bullet) |
| 3 | Related Work | 1.0 | Desktop pets/agents (Bonzi Buddy, Clippy, Live2D companions), LLM desktop assistants, gamification trong quản lý hệ thống, multi-provider LLM abstraction |
| 4 | System Architecture | 1.5 | Kiến trúc tổng thể (Qt6/C++), AiClient interface trừu tượng hóa 3 provider, luồng dữ liệu pet state ↔ hunger manager ↔ recycle bin manager |
| 5 | Core Mechanisms | 2.0 | (a) Hunger/Size state machine, (b) thuật toán "ăn rác" qua COM API (IEnumShellItems/IFileOperation), (c) chính sách chọn file (cũ nhất/lớn nhất), ngưỡng bỏ qua file >1GB, (d) multi-LLM dispatch & retry logic (503/429) |
| 6 | Implementation | 0.7 | Stack cụ thể, thách thức kỹ thuật (COM lifecycle, animation theo state, đóng gói windeployqt) |
| 7 | Evaluation | 1.0 | Đo lường: độ trễ phản hồi LLM theo provider, hiệu năng enumerate/xóa file theo số lượng/kích thước, khảo sát trải nghiệm người dùng (nếu có), so sánh dung lượng giải phóng thực tế |
| 8 | Discussion / Limitations | 0.3 | Giới hạn nền tảng (Windows-only, MinGW), API key handling, hướng mở rộng |
| 9 | Conclusion & Future Work | 0.2 | Tóm tắt đóng góp, hướng phát triển (cross-platform, local LLM, đa pet) |
| — | References | (không tính vào 8 trang chính, thêm 0.5–1 trang) | 15–25 tài liệu tham khảo |

*Tổng ~8 trang thân bài, cộng phụ lục/tài liệu tham khảo nếu còn chỗ.*

## 3. Đóng góp khoa học cần làm rõ (Contributions, dùng cho Abstract + Intro)

1. **Kiến trúc AiClient thống nhất** cho phép hoán đổi runtime giữa Gemini/GPT/Claude mà không đổi logic ứng dụng (generalizable pattern cho multi-LLM desktop app).
2. **Cơ chế gamification quản lý ổ đĩa**: ánh xạ dung lượng file (MB) → chỉ số "hunger"/kích thước pet, biến việc dọn Recycle Bin thành tương tác có phản hồi trực quan.
3. **Truy xuất/liệt kê tài nguyên hệ thống hiệu quả** qua COM API (IEnumShellItems) với chính sách lựa chọn file có thể cấu hình — liên hệ trực tiếp tới "efficient information retrieval" ở cấp hệ điều hành thay vì văn bản/dữ liệu truyền thống.

## 4. Dữ liệu/thực nghiệm cần chuẩn bị trước khi viết Section 7

- [ ] Benchmark thời gian phản hồi trung bình của 3 provider (n mẫu, cùng prompt) — bảng so sánh
- [ ] Benchmark thời gian enumerate + xóa N file trong Recycle Bin (N = 10/100/1000), biểu đồ thời gian theo N
- [ ] Test case file >1GB bị skip — log minh chứng
- [ ] (Tùy chọn) khảo sát nhỏ 5–10 người dùng thử nghiệm, đánh giá mức độ hứng thú/tiện dụng (Likert scale)
- [ ] Screenshot/sơ đồ kiến trúc (Figure 1: architecture diagram), sơ đồ state machine hunger (Figure 2)

## 5. Việc cần làm trước khi nộp

- [ ] Tải Springer template (Word/LaTeX) từ trang ICEIR, format đúng chuẩn
- [ ] Ẩn/loại bỏ toàn bộ API key khỏi hình ảnh/code snippet minh họa
- [ ] Xác định đồng tác giả (nếu có) + affiliation
- [ ] Chuẩn bị 2 sơ đồ (architecture + state machine) — có thể dùng draw.io/Mermaid rồi export
- [ ] Viết Related Work: tìm 8–12 paper liên quan (desktop agents, gamification, LLM tool orchestration, file system management) qua Google Scholar/IEEE/ACM
- [ ] Chạy benchmark thực tế (mục 4) trước khi viết Evaluation, tránh số liệu giả định
- [ ] Kiểm tra deadline nộp bài + registration fee trên trang ICEIR (cần xác nhận lại vì trang có thể yêu cầu đăng nhập/cert riêng khi truy cập)

## 6. Rủi ro / lưu ý

- Trang iceir.nctu.edu.vn gặp lỗi SSL certificate khi fetch tự động (`unable to verify the first certificate`) — cần **tự vào trình duyệt kiểm tra trực tiếp** để lấy chính xác: deadline, phí, danh sách topic đầy đủ, template link trước khi triển khai viết.
- Nội dung "gamification" (pet ăn rác) là điểm khác biệt/thú vị nhưng ban giám khảo có thể đánh giá thiếu "rigor" — cần nhấn mạnh phần Evaluation định lượng (mục 4) để cân bằng.
- Nếu không đủ dữ liệu thực nghiệm, cân nhắc nộp dạng "system/demo paper" thay vì "research paper" nếu ICEIR có track riêng (cần xác nhận trên site).
