from fastapi import FastAPI, Depends, Request, HTTPException
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
# Dấu chấm (.) nghĩa là "ngay tại thư mục này"
from . import models, database, crud, schemas
from . import models, schemas
from sqlalchemy.orm import Session
# 1 dấu chấm (.): Nghĩa là "ngay trong thư mục hiện tại này".

# 2 dấu chấm (..): Nghĩa là "nhảy ra ngoài thư mục cha".
from . import crud
from dotenv import load_dotenv
import os

load_dotenv()

# Tạo bảng tự động
models.Base.metadata.create_all(bind=database.engine)

app = FastAPI(title="Light Sensor System")

# Cấu hình templates
templates = Jinja2Templates(directory="app/templates")

# Dependency lấy DB Session
def get_db():
    db = database.SessionLocal()
    try:
        yield db
    finally:
        db.close()

# --- API ENDPOINTS ---

# 1. API nhận dữ liệu từ ESP32
@app.post("/api/sensor", status_code=201)
def receive_sensor(data: schemas.SensorIn, db: Session = Depends(get_db)):
    if data.lux < 0:
        raise HTTPException(status_code=400, detail="Lux cannot be negative")
    
    item = crud.create_reading(db, data)
    return {"status": "success", "id": item.id}

# 2. API lấy dữ liệu JSON (cho Chart vẽ)
@app.get("/api/readings", response_model=list[schemas.SensorOut])
def api_readings(limit: int = 50, db: Session = Depends(get_db)):
    # Lấy 50 dòng mới nhất
    rows = crud.get_recent(db, limit=limit)
    # Đảo ngược lại để vẽ biểu đồ từ trái sang phải (Cũ -> Mới)
    return rows[::-1] 

# 3. Giao diện Dashboard (HTML)
@app.get("/", response_class=HTMLResponse)
def dashboard(request: Request, db: Session = Depends(get_db)):
    # Lấy dữ liệu ban đầu để render server-side (nếu cần)
    return templates.TemplateResponse("index.html", {"request": request})