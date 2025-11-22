from sqlalchemy.orm import Session
# Session là kết nối đến cơ sở dữ liệu, dùng để thêm, truy vấn, xóa, commit dữ liệu.
from . import models, schemas
# Import các module nội bộ của project

# hàm tạo bản ghi mới trong bảng SensorReading.
def create_reading(db: Session, reading: schemas.SensorIn):
    # db: Session → kết nối database để thao tác.
    # reading: schemas.SensorIn → dữ liệu đầu vào được xác thực qua Pydantic schema SensorIn.
    db_item = models.SensorReading(
        # Tạo object ORM mới db_item thuộc bảng SensorReading
        # Lúc này db_item chưa được lưu vào database.
        device_id=reading.device_id, # gán device ID từ request
        lux=reading.lux #gán giá trị lux từ request.
    )
    db.add(db_item) #Thêm object db_item vào session của SQLAlchemy.chỉ năm bộ nhớ tạm
    db.commit() #ghi dữ liệu từ session vào cơ sở dữ liệu thực.
    db.refresh(db_item) #Refresh object từ database để cập nhật các giá trị tự động sinh
    return db_item

# Hàm lấy các bản ghi gần đây nhất từ bảng SensorReading
def get_recent(db: Session, limit: int = 100):
    return db.query(models.SensorReading).order_by(models.SensorReading.timestamp.desc()).limit(limit).all()
