from fastapi import FastAPI, Depends, Request, HTTPException
from fastapi.responses import HTMLResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from sqlalchemy.orm import Session
from dotenv import load_dotenv
import csv
import io
import os
from datetime import datetime

# Import các module nội bộ
from . import models, database, crud, schemas

# Load biến môi trường
load_dotenv()

# Tạo bảng trong DB nếu chưa có
models.Base.metadata.create_all(bind=database.engine)

app = FastAPI(title="Light Sensor System")

# Cấu hình folder chứa giao diện (HTML)
templates = Jinja2Templates(directory="app/templates")

# Hàm lấy DB Session
def get_db():
    db = database.SessionLocal()
    try:
        yield db
    finally:
        db.close()

# --- CÁC API ---

# 1. API NHẬN DỮ LIỆU TỪ ESP32 (Giữ nguyên)
@app.post("/api/sensor", status_code=201)
def receive_sensor(data: schemas.SensorIn, db: Session = Depends(get_db)):
    if data.lux < 0:
        raise HTTPException(status_code=400, detail="Lux cannot be negative")
    item = crud.create_reading(db, data)
    return {"status": "success", "id": item.id}

# 2. API CHO BIỂU ĐỒ (Chỉ lấy 20 điểm mới nhất)
@app.get("/api/readings/recent", response_model=list[schemas.SensorOut])
def api_readings_recent(limit: int = 20, db: Session = Depends(get_db)):
    # Lấy dữ liệu mới nhất
    rows = crud.get_recent(db, limit=limit)
    # Đảo ngược lại (Cũ -> Mới) để vẽ biểu đồ chạy từ trái sang phải
    return rows[::-1]

# 3. API CHO BẢNG DỮ LIỆU (Có phân trang)
@app.get("/api/readings")
def api_readings_paginated(page: int = 1, limit: int = 20, db: Session = Depends(get_db)):
    skip = (page - 1) * limit
    # Gọi hàm phân trang bên crud.py (Cậu nhớ cập nhật crud.py như tớ bảo ở tin nhắn trước nhé)
    data, total_count = crud.get_readings_paginated(db, skip=skip, limit=limit)
    
    return {
        "data": data,
        "total": total_count,
        "page": page,
        "limit": limit,
        "total_pages": (total_count + limit - 1) // limit
    }

# 4. API XUẤT CSV (Theo khoảng thời gian)
@app.get("/api/export-csv")
def export_csv(start: str, end: str, db: Session = Depends(get_db)):
    try:
        start_date = datetime.strptime(start, "%Y-%m-%d")
        end_date = datetime.strptime(end + " 23:59:59", "%Y-%m-%d %H:%M:%S")
    except ValueError:
        return {"error": "Sai định dạng ngày. Hãy dùng YYYY-MM-DD"}

    readings = crud.get_readings_by_date_range(db, start_date, end_date)

    # Tạo file CSV trong bộ nhớ
    stream = io.StringIO()
    csv_writer = csv.writer(stream)
    csv_writer.writerow(["ID", "Thiet Bi (MAC)", "Do sang (Lux)", "Thoi gian"]) # Header
    
    for row in readings:
        csv_writer.writerow([row.id, row.device_id, row.lux, row.timestamp])
    
    stream.seek(0)
    response = StreamingResponse(iter([stream.getvalue()]), media_type="text/csv")
    response.headers["Content-Disposition"] = "attachment; filename=sensor_data.csv"
    return response

# 5. GIAO DIỆN CHÍNH
@app.get("/", response_class=HTMLResponse)
def dashboard(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})