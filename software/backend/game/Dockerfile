FROM python:3.9
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY play.py .
COPY game_record.py .
COPY main.py .
CMD ["python", "main.py"]
