# Claw4 family backend container image (CP5 static config).
# Built from the repository root. Not executed on this host (no Docker);
# provided so a deploy target can run the same code against PostgreSQL.
FROM python:3.13-slim

WORKDIR /srv/claw4

COPY backend/requirements.txt backend/requirements.txt
# PostgreSQL driver for the deployment dialect (tests keep SQLite).
RUN pip install --no-cache-dir -r backend/requirements.txt psycopg[binary]

COPY backend/app backend/app

EXPOSE 8000
CMD ["uvicorn", "app.main:app", "--host", "127.0.0.1", "--port", "8000"]
