from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, declarative_base
import os
from dotenv import load_dotenv

load_dotenv()

# Lấy URL từ biến môi trường. Nếu không có thì báo lỗi luôn chứ không dùng hardcode.
MYSQL_URL = os.getenv("MYSQL_URL")

if not MYSQL_URL:
    raise ValueError("CHUA CAU HINH MYSQL_URL TRONG FILE .env !!")

# pool_pre_ping=True giúp tự động kết nối lại nếu MySQL bị timeout (lỗi MySQL Server has gone away)
engine = create_engine(MYSQL_URL, pool_pre_ping=True)

SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()