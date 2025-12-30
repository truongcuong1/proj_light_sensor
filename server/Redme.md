python -m venv venv
.\venv\Scripts\activate
pip install -r requirement.txt
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload


👉 1. Mở MySQL với tài khoản root
mysql -u root -p
echo %MYSQL_URL%;
setx MYSQL_URL ""
CREATE DATABASE lightdb22;
GRANT ALL PRIVILEGES ON lightdb22.* TO 'sensor'@'%' IDENTIFIED BY 'anhcuong';
FLUSH PRIVILEGES;

Nhập mật khẩu root.

👉 2. Cấp quyền cho user sensor đối với database lightdb2

Nếu cậu đang dùng database tên lightdb2 trong file .env hoặc trong code, chạy lệnh sau:

GRANT ALL PRIVILEGES ON lightdb22.* TO 'sensor'@'%' IDENTIFIED BY 'anhcuong';
FLUSH PRIVILEGES;

Kết quả nên giống như:

GRANT USAGE ON *.* TO `sensor`@`%`
GRANT ALL PRIVILEGES ON `lightdb2`.* TO `sensor`@`%`


Cậu làm theo 2 bước sau nhé:


🩵 2️⃣ Cấp quyền truy cập vào database lightdb2
GRANT ALL PRIVILEGES ON lightdb2.* TO 'sensor'@'%';
FLUSH PRIVILEGES;

🩵 3️⃣ (Tuỳ chọn) Kiểm tra lại
SHOW GRANTS FOR 'sensor'@'%';


Kết quả mong đợi:

GRANT USAGE ON *.* TO `sensor`@`%`;
GRANT ALL PRIVILEGES ON `lightdb22`.* TO `sensor`@`%`

⚙️ Nếu database lightdb2 chưa tồn tại

Tạo nó trước khi cấp quyền:

CREATE DATABASE lightdb22;
DROP DATABASE ten_bang_muon xoa; - xóa database

-- Xem các database
SHOW DATABASES;

-- Chọn database để dùng
USE lightdb22;

-- Xem các bảng trong database hiện tại
SHOW TABLES;
-- xem tất cả dữ liệu trong bảng db 
SELECT * FROM lightdb22;

root là gốc
-- Xem cấu trúc (cột) của bảng
DESCRIBE sensor_readings;

-- hoặc
SHOW COLUMNS FROM your_table;

-- Xem cột chi tiết từ information_schema
SELECT COLUMN_NAME, DATA_TYPE, IS_NULLABLE, COLUMN_DEFAULT
FROM INFORMATION_SCHEMA.COLUMNS
WHERE TABLE_SCHEMA = 'your_db' AND TABLE_NAME = 'your_table';

-- Xem vài dòng mẫu
SELECT * FROM your_table LIMIT 10;
Câu hỏi rất tinh tế! Đây là sự nhầm lẫn cực kỳ phổ biến khi mới làm việc với Database.

Cậu đang hiểu nhầm khái niệm "Chìa khóa". Để tớ đính chính lại cho đúng tư duy hệ thống nhé:

1. Sự thật: Mật khẩu KHÔNG PHẢI là "Chìa khóa"
Tên đăng nhập (User - ví dụ sensor): Đây mới chính là cái Thẻ Định Danh (ID Card) của nhân viên.

Mật khẩu (anhcuong): Chỉ là Mã PIN để xác nhận thẻ đó là thật.

Quyền hạn (Grants): Là danh sách các "phòng" mà cái Thẻ Định Danh đó được phép quẹt vào.

Ví dụ thực tế:

Cậu có user sensor (pass: anhcuong).

Cậu có user admin (pass cũng là anhcuong).

Dù Mã PIN giống hệt nhau, nhưng khi cậu đăng nhập bằng thẻ sensor, bảo vệ (MySQL) tra sổ thấy thẻ này chỉ được vào phòng lightdb. Còn khi đăng nhập bằng thẻ admin, bảo vệ tra sổ thấy được vào tất cả các phòng.

=> Kết luận: MySQL cấp quyền dựa trên Tên User, không dựa trên Mật khẩu.

2. Câu lệnh để xem "Ai được vào nhà nào?"
Để biết chính xác user nào ("chìa khóa" nào) được gán với database nào ("nhà" nào), cậu hãy chạy câu lệnh này trong MySQL Terminal:

SQL

SELECT user, host, db, select_priv, insert_priv, update_priv FROM mysql.db;

Để làm "trắng" dữ liệu (xóa sạch dữ liệu cũ nhưng giữ nguyên cấu trúc bảng để nhập mới từ đầu), bạn có 2 lựa chọn tùy vào việc bạn muốn nó "mới" đến mức nào.

Dưới đây là giải thích và cách làm:

Cách 1: Dùng lệnh TRUNCATE (Khuyên dùng - Chuẩn nhất)
Đây là cách tốt nhất để biến một bảng dữ liệu trở về trạng thái "như chưa từng được sử dụng".

Tác dụng: Xóa sạch toàn bộ dữ liệu.

Điểm đặc biệt: Nó sẽ reset (đặt lại) bộ đếm ID tự động tăng (Auto Increment) về 1.

Ví dụ: Nếu bạn dùng lệnh DELETE bình thường, khi bạn xóa hết dữ liệu cũ đi, dữ liệu mới nhập vào sẽ có ID tiếp theo (ví dụ 101) chứ không phải là 1. Nhưng TRUNCATE sẽ đưa ID về 1.

Câu lệnh SQL:

SQL

TRUNCATE TABLE ten_bang_cua_ban;
TRUNCATE TABLE sensor_readings;
Cách 2: Thao tác trên giao diện (phpMyAdmin)
Nếu bạn dùng XAMPP/phpMyAdmin, bạn có thể làm sạch toàn bộ các bảng cùng một lúc chỉ với vài cú click chuột mà không cần gõ lệnh cho từng bảng.

Các bước thực hiện:

Vào phpMyAdmin, chọn cái Database bạn muốn xóa trắng (cột bên trái).

Kéo xuống dưới cùng danh sách các bảng, bạn sẽ thấy dòng "Check all" (Chọn tất cả). Tích vào đó.

Ở menu thả xuống (thường ghi là "With selected..." hoặc "Với các mục đã chọn..."), bạn chọn Empty (Làm rỗng) hoặc Truncate.

Lưu ý: Đừng chọn "Drop" (Xóa), vì Drop là xóa luôn cả bảng, mất luôn cấu trúc.

Hệ thống sẽ hỏi xác nhận "Bạn có muốn thực thi câu lệnh TRUNCATE...", bấm Yes/Go.

Cách 3: Xóa Database và Tạo lại (Cách "Cục súc")
Nếu database của bạn quá lộn xộn hoặc bạn muốn thay đổi cả cấu trúc, cách nhanh nhất là xóa sổ nó đi và tạo lại (nếu bạn còn giữ file code tạo bảng hoặc file SQL gốc).

Câu lệnh SQL:

SQL

DROP DATABASE ten_database_cu;
CREATE DATABASE ten_database_moi;
⚠️ Một lưu ý quan trọng về "Khóa Ngoại" (Foreign Keys)
Nếu các bảng của bạn có liên kết với nhau (bảng A nối với bảng B), khi bạn chạy lệnh TRUNCATE có thể sẽ bị lỗi do ràng buộc dữ liệu.

Để giải quyết, bạn cần tắt kiểm tra khóa ngoại trước khi xóa, rồi bật lại:

SQL

-- 1. Tắt kiểm tra khóa ngoại
SET FOREIGN_KEY_CHECKS = 0;

-- 2. Xóa trắng các bảng
TRUNCATE TABLE bang_A;
TRUNCATE TABLE bang_B;

-- 3. Bật lại kiểm tra khóa ngoại (Rất quan trọng, đừng quên!)
SET FOREIGN_KEY_CHECKS = 1;
Bạn đang dùng phpMyAdmin hay gõ lệnh trực tiếp? Nếu gặp lỗi "Cannot truncate a table referenced in a foreign key constraint" thì làm theo phần lưu ý trên nhé.
Để thực hiện xóa dữ liệu, trước tiên bạn phải chắc chắn mình đang ở trong database đó (USE lightdb22;) và biết chính xác Tên Bảng (xem bằng SHOW TABLES;).

Giả sử tên bảng của bạn tìm được là esp32_data. Dưới đây là 3 cấp độ xóa, bạn chọn cái nào phù hợp nhu cầu nhé:

Cấp độ 1: Xóa có chọn lọc (Xóa dòng bị sai)
Dùng khi bạn chỉ muốn xóa một vài dòng dữ liệu rác, giữ lại dữ liệu tốt.

Câu lệnh:

SQL

DELETE FROM esp32_data WHERE id = 5;
WHERE id = 5: Chỉ xóa dòng có ID là 5.

Lưu ý cực quan trọng: Nếu bạn quên WHERE, nó sẽ xóa sạch dữ liệu cả bảng đấy!

Cấp độ 2: Xóa sạch sẽ làm lại từ đầu (Reset 100%)
Dùng khi bạn test xong rồi, muốn xóa hết dữ liệu cũ để ESP32 gửi dữ liệu mới vào, ID bắt đầu lại từ 1.

Câu lệnh:

SQL

TRUNCATE TABLE esp32_data;
Tác dụng: Xóa sạch dữ liệu + Reset ID về 1.

Ưu điểm: Nhanh, gọn, sạch sẽ.

Cấp độ 3: Xóa sạch nhưng KHÔNG Reset ID
Dùng khi bạn muốn xóa hết dữ liệu cũ, nhưng muốn ID của dữ liệu mới tiếp nối số cũ (ví dụ cũ là 100, mới nhập vào sẽ là 101).

Câu lệnh:

SQL

DELETE FROM esp32_data;
Khác biệt: Giống TRUNCATE là xóa hết, nhưng ID không quay về 1.

Cấp độ 4: Xóa luôn cái bảng (Hủy diệt)
Dùng khi bạn thiết kế bảng bị sai cột, muốn đập đi xây lại bảng mới.

Câu lệnh:

SQL

DROP TABLE esp32_data;
Hậu quả: Mất cả dữ liệu lẫn cấu trúc bảng. Bạn phải dùng lệnh CREATE TABLE để tạo lại.

Tóm lại: Nếu bạn muốn làm mới để chạy thật thì dùng lệnh TRUNCATE TABLE ten_bang; là chuẩn nhất nhé! Bạn gõ thử lệnh đó xem được chưa?