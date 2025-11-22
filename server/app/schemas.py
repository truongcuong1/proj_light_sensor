from pydantic import BaseModel
from typing import Optional
from datetime import datetime

# Schema dữ liệu nhận từ ESP32
class SensorIn(BaseModel):
    device_id: str
    lux: float
    # ts có thể null, server sẽ tự điền giờ hiện tại
    ts: Optional[datetime] = None 

# Schema trả về cho Web/API
class SensorOut(BaseModel):
    id: int
    device_id: str
    lux: float
    timestamp: datetime

    # Cấu hình cho Pydantic V2 (thay cho orm_mode = True cũ)
    class Config:
        from_attributes = True