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
-- Xem các database
SHOW DATABASES;

-- Chọn database để dùng
USE lightdb22;

-- Xem các bảng trong database hiện tại
SHOW TABLES;

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