from sqlalchemy.orm import Session
from . import models, schemas
from datetime import datetime

# --- 1. HÀM TẠO BẢN GHI MỚI ---
def create_reading(db: Session, reading: schemas.SensorIn):
    # Tạo object ORM mới (chưa lưu vào DB)
    db_reading = models.SensorReading(
        device_id=reading.device_id,
        lux=reading.lux
    )
    # Thêm vào session (bộ nhớ tạm)
    db.add(db_reading)
    # Commit để lưu chính thức vào MySQL
    db.commit()
    # Refresh để lấy lại ID và Timestamp tự sinh từ MySQL
    db.refresh(db_reading)
    return db_reading

# --- 2. HÀM LẤY DỮ LIỆU CHO BIỂU ĐỒ (MỚI NHẤT) ---
def get_recent(db: Session, limit: int = 100):
    return db.query(models.SensorReading)\
             .order_by(models.SensorReading.timestamp.desc())\
             .limit(limit)\
             .all()

# --- 3. HÀM LẤY DỮ LIỆU PHÂN TRANG (CHO BẢNG) ---
def get_readings_paginated(db: Session, skip: int = 0, limit: int = 20):
    # .offset(skip): Bỏ qua 'skip' dòng đầu
    # .limit(limit): Chỉ lấy 'limit' dòng
    # .order_by(...desc()): Sắp xếp mới nhất lên đầu
    data = db.query(models.SensorReading)\
             .order_by(models.SensorReading.id.desc())\
             .offset(skip)\
             .limit(limit)\
             .all()
    
    # Đếm tổng số bản ghi (để tính số trang)
    total = db.query(models.SensorReading).count()
    
    # Trả về cả dữ liệu và tổng số dòng
    return data, total

# --- 4. HÀM LẤY DỮ LIỆU THEO NGÀY (CHO CSV) ---
def get_readings_by_date_range(db: Session, start_date: datetime, end_date: datetime):
    return db.query(models.SensorReading)\
             .filter(models.SensorReading.timestamp >= start_date, 
                     models.SensorReading.timestamp <= end_date)\
             .order_by(models.SensorReading.timestamp.desc())\
             .all()