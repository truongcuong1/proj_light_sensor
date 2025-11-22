# this file chứa class ORM ánh xạ các bảng trong database (ví dụ SensorReading).

# các kiểu cột trong bảng database.
from sqlalchemy import Column, Integer, String, Float, DateTime, func
# Base lớp cơ sở của SQLAlchemy ORM, tất cả class ORM phải kế thừa để được ánh xạ vào database
from .database import Base

# Tạo class SensorReading ánh xạ bảng sensor_readings trong MySQL.
class SensorReading(Base):
    __tablename__ = "sensor_readings"
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), index=True)
    lux = Column(Float, nullable=False)
    timestamp = Column(DateTime(timezone=False), server_default=func.now(), index=True)
