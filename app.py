import streamlit as st
import paho.mqtt.client as mqtt
import time

# Konfiguraasi MQTT
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

st.set_page_config(page_title="Smart Cage Dashboard", page_icon="🐔", layout="wide")

# Tampilan tabel pada mobile
st.markdown("""
<style>
table {
    border-collapse: collapse !important;
    width: 100% !important;
}
th, td {
    border: 1px solid rgba(128, 128, 128, 0.6) !important;
    padding: 8px !important;
}
th {
    background-color: rgba(128, 128, 128, 0.1) !important;
}
</style>
""", unsafe_allow_html=True)

# Inisialisasi Koneksi MQTT
@st.cache_resource
def get_mqtt_client():
    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    except AttributeError:
        client = mqtt.Client()
        
    client.mqtt_data = {
        'suhu': '--',
        'kelembapan': '--',
        'relay': '--',
        'servo': '--',
        'jam': '--',
        'kategori': '--'
    }

    # Subscribe ke Topik MQTT
    def on_connect(client, userdata, flags, rc, *args):
        if rc == 0:
            client.subscribe("monitoring/suhu")
            client.subscribe("monitoring/kelembapan")
            client.subscribe("monitoring/relay")
            client.subscribe("monitoring/servo")
            client.subscribe("monitoring/jam")
            client.subscribe("monitoring/kategori")

    # Callback saat pesan diterima
    def on_message(client, userdata, msg):
        topic = msg.topic
        try:
            payload = msg.payload.decode("utf-8")
            if topic == "monitoring/suhu":
                client.mqtt_data['suhu'] = payload
            elif topic == "monitoring/kelembapan":
                client.mqtt_data['kelembapan'] = payload
            elif topic == "monitoring/relay":
                client.mqtt_data['relay'] = payload
            elif topic == "monitoring/servo":
                client.mqtt_data['servo'] = payload
            elif topic == "monitoring/jam":
                client.mqtt_data['jam'] = payload
            elif topic == "monitoring/kategori":
                client.mqtt_data['kategori'] = payload
        except Exception:
            pass

    client.on_connect = on_connect
    client.on_message = on_message
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
    except Exception as e:
        print(f"Error connecting to MQTT: {e}")
    return client

mqtt_client = get_mqtt_client()

def publish_command(topic, message):
    if mqtt_client:
        mqtt_client.publish(topic, message)

# Sidebar
with st.sidebar:
    st.header("📋 Kategori & Jadwal")
    
    st.subheader("Tabel Kategori Suhu")
    st.markdown("""
    | Kategori | Usia (Hari) | Suhu Ideal (°C) |
    |---|---|---|
    | 1 | 0 – 6 | 30 – 34 |
    | 2 | 7 – 13 | 28 – 32 |
    | 3 | 14 | 25 – 29 |
    | 4 | > 14 | 21 – 25 |
    """)
    
    st.subheader("Pilih Kategori")
    active_kategori = mqtt_client.mqtt_data.get('kategori', '--') if mqtt_client else '--'
    
    col1, col2 = st.columns(2)
    with col1:
        if st.button("Kategori 1", use_container_width=True, type="primary" if active_kategori == "1" else "secondary"):
            publish_command("control/kategori", "1")
            if mqtt_client: mqtt_client.mqtt_data['kategori'] = "1"
            st.rerun()
        if st.button("Kategori 2", use_container_width=True, type="primary" if active_kategori == "2" else "secondary"):
            publish_command("control/kategori", "2")
            if mqtt_client: mqtt_client.mqtt_data['kategori'] = "2"
            st.rerun()
    with col2:
        if st.button("Kategori 3", use_container_width=True, type="primary" if active_kategori == "3" else "secondary"):
            publish_command("control/kategori", "3")
            if mqtt_client: mqtt_client.mqtt_data['kategori'] = "3"
            st.rerun()
        if st.button("Kategori 4", use_container_width=True, type="primary" if active_kategori == "4" else "secondary"):
            publish_command("control/kategori", "4")
            if mqtt_client: mqtt_client.mqtt_data['kategori'] = "4"
            st.rerun()
            
    st.divider()
    st.subheader("Tabel Jadwal Pakan")
    st.markdown("""
    | Sesi | Waktu |
    |---|---|
    | Pagi | 07:00 |
    | Siang | 12:00 |
    | Sore | 17:00 |
    """)

# Main Content
st.title("🐔 Dashboard Smart Cage IoT")
st.markdown("Monitoring Suhu, Kelembapan, dan Kontrol Pakan/Relay secara Real-time.")

@st.fragment(run_every="1s")
def display_monitoring():
    data = mqtt_client.mqtt_data if mqtt_client else {}
    suhu = data.get('suhu', '--')
    kelembapan = data.get('kelembapan', '--')
    relay = data.get('relay', '--')
    servo = data.get('servo', '--')
    jam = data.get('jam', '--')
    kategori = data.get('kategori', '--')

    col1, col2, col3, col4 = st.columns(4)
    with col1:
        st.metric(label="🌡️ Suhu", value=f"{suhu} °C")
    with col2:
        st.metric(label="💧 Kelembapan", value=f"{kelembapan} %")
    with col3:
        st.metric(label="⚡ Relay State", value=relay)
    with col4:
        st.metric(label="⏰ Jadwal Pakan", value=servo)

    status_suhu = "--"
    try:
        s = float(suhu)
        k = int(kategori)
        if k == 1: min_t, max_t = 30, 34
        elif k == 2: min_t, max_t = 28, 32
        elif k == 3: min_t, max_t = 25, 29
        elif k == 4: min_t, max_t = 21, 25
        else: min_t, max_t = 0, 100
        
        if s > max_t: status_suhu = "🔴 Panas"
        elif s < min_t: status_suhu = "🟡 Dingin"
        else: status_suhu = "🟢 Ideal"
    except ValueError:
        pass

    st.divider()

    st.markdown(f"### 🕒 Waktu: `{jam}`\n### 🏷️ Kategori: `{kategori}`\n### 🌡️ Status Suhu: `{status_suhu}`")
display_monitoring()

st.divider()

st.header("Kontrol Manual")
col_relay, col_servo = st.columns(2)

with col_relay:
    st.subheader("Kontrol Relay (Fan)")
    st.markdown("Pilih mode relay:")
    rc1, rc2, rc3 = st.columns(3)
    with rc1:
        if st.button("Relay ON", use_container_width=True):
            publish_command("control/relay", "ON")
            st.toast("Perintah Relay ON terkirim!")
    with rc2:
        if st.button("Relay OFF", use_container_width=True):
            publish_command("control/relay", "OFF")
            st.toast("Perintah Relay OFF terkirim!")
    with rc3:
        if st.button("Mode AUTO", use_container_width=True):
            publish_command("control/relay", "AUTO")
            st.toast("Perintah Mode AUTO terkirim!")

with col_servo:
    st.subheader("Kontrol Pakan (Servo)")
    st.markdown("Berikan pakan secara manual sekarang:")
    if st.button("Memberi Pakan", type="primary", use_container_width=True):
        publish_command("control/servo", "FEED")
        st.toast("Perintah Memberi Pakan terkirim!")
