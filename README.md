# IoT Rumah WebUI 🏠

Sebuah aplikasi web interaktif untuk mengendalikan dan memonitor perangkat IoT di rumah pintar Anda. Aplikasi ini menyediakan antarmuka user-friendly yang memungkinkan Anda mengelola berbagai perangkat rumah pintar dari satu dashboard terpusat.

## 📋 Fitur Utama

- 🎛️ **Dashboard Terpusat** - Pantau dan kontrol semua perangkat IoT dari satu tempat
- 💡 **Kontrol Perangkat Real-time** - Aktifkan/nonaktifkan perangkat secara instan
- 📊 **Monitoring Status** - Lihat status real-time semua perangkat yang terhubung
- 🔐 **Aman** - Dengan autentikasi dan enkripsi komunikasi
- 📱 **Responsive Design** - Bekerja sempurna di desktop, tablet, dan mobile
- ⚡ **Performa Tinggi** - Interface yang responsif dengan loading cepat

## 🛠️ Teknologi yang Digunakan

| Teknologi | Persentase | Deskripsi |
|-----------|-----------|-----------|
| **C++** | 38.4% | Backend dan kontrol perangkat |
| **JavaScript** | 24% | Logika frontend dan interaktivitas |
| **CSS** | 24.5% | Styling dan desain responsif |
| **HTML** | 13.1% | Struktur dan markup halaman |

## 📦 Prasyarat

Sebelum memulai, pastikan Anda telah menginstal:

- [Git](https://git-scm.com/)
- [Node.js](https://nodejs.org/) (versi 14 atau lebih tinggi)
- [npm](https://www.npmjs.com/) atau [yarn](https://yarnpkg.com/)
- Compiler C++ (GCC, Clang, atau MSVC untuk backend)

## 🚀 Instalasi

1. **Clone Repository**
   ```bash
   git clone https://github.com/Wahyunugroho99/iot-rumah-webui.git
   cd iot-rumah-webui
   ```

2. **Install Dependencies**
   ```bash
   npm install
   # atau
   yarn install
   ```

3. **Konfigurasi Environment**
   ```bash
   cp .env.example .env
   # Edit .env dengan konfigurasi Anda
   ```

4. **Build Backend (C++)**
   ```bash
   # Sesuaikan dengan sistem build Anda
   cmake .
   make
   ```

5. **Jalankan Aplikasi**
   ```bash
   npm start
   # atau
   yarn start
   ```

6. **Akses Aplikasi**
   Buka browser dan navigasi ke `http://localhost:3000`

## 📝 Penggunaan

### Dashboard
Setelah login, Anda akan melihat dashboard utama dengan:
- Daftar semua perangkat yang terhubung
- Status real-time setiap perangkat
- Control panel untuk mengubah setting

### Kontrol Perangkat
1. Klik pada perangkat yang ingin Anda kontrol
2. Ubah setting sesuai kebutuhan
3. Perubahan akan langsung diterapkan

### Monitoring
- Lihat riwayat aktivitas perangkat
- Analisis penggunaan energi
- Set alert untuk kondisi tertentu

## 🔧 Struktur Proyek

```
iot-rumah-webui/
├── src/
│   ├── cpp/                 # Backend C++
│   │   ├── main.cpp
│   │   ├── device_manager/
│   │   └── ...
│   ├── js/                  # JavaScript Frontend
│   │   ├── components/
│   │   ├── utils/
│   │   └── app.js
│   ├── css/                 # Stylesheet
│   │   ├── styles.css
│   │   └── responsive.css
│   └── index.html           # Entry point HTML
├── public/                  # Asset statis
├── config/                  # Konfigurasi
├── package.json
├── CMakeLists.txt
└── README.md
```

## 🔌 Koneksi Perangkat

Aplikasi ini mendukung koneksi dengan:
- **WiFi IoT Devices** - Dengan protokol standar IoT
- **Bluetooth Devices** - Perangkat Bluetooth lokal
- **MQTT** - Untuk IoT devices yang menggunakan protokol MQTT

### Konfigurasi Perangkat

Edit file `config/devices.json` untuk menambah perangkat baru:

```json
{
  "devices": [
    {
      "id": "device_001",
      "name": "Lampu Ruang Tamu",
      "type": "light",
      "protocol": "wifi",
      "ip": "192.168.1.100"
    }
  ]
}
```

## 📚 API Reference

### GET /api/devices
Mendapatkan daftar semua perangkat

### POST /api/devices/:id/control
Mengontrol perangkat tertentu

```json
{
  "action": "on/off",
  "value": true
}
```

### GET /api/devices/:id/status
Mendapatkan status real-time perangkat

## 🐛 Troubleshooting

### Perangkat tidak terdeteksi
- Pastikan perangkat dalam jangkauan WiFi/Bluetooth
- Restart aplikasi
- Periksa konfigurasi network

### Interface lambat
- Bersihkan browser cache
- Kurangi jumlah perangkat yang dipantau secara bersamaan
- Periksa koneksi internet

### Error koneksi backend
- Pastikan backend C++ berjalan dengan baik
- Cek port yang digunakan tidak bentrok
- Lihat log error di console

## 🤝 Kontribusi

Kami menyambut kontribusi Anda! Untuk berkontribusi:

1. Fork repository ini
2. Buat branch fitur (`git checkout -b feature/AmazingFeature`)
3. Commit perubahan Anda (`git commit -m 'Add some AmazingFeature'`)
4. Push ke branch (`git push origin feature/AmazingFeature`)
5. Buka Pull Request

## 📝 Lisensi

Proyek ini saat ini tidak memiliki lisensi yang ditentukan. Silakan tambahkan lisensi yang sesuai jika diperlukan.

## 📧 Kontak & Support

Jika Anda memiliki pertanyaan atau saran, silakan:
- Buka [Issues](https://github.com/Wahyunugroho99/iot-rumah-webui/issues)
- Hubungi melalui GitHub

## 🎯 Roadmap

- [ ] Integrasi dengan smart home platforms (Google Home, Alexa)
- [ ] Sistem notifikasi push
- [ ] Machine learning untuk automation
- [ ] Analitik penggunaan energi
- [ ] Aplikasi mobile native
- [ ] Cloud sync dan backup

## 📦 Dependencies

Lihat `package.json` dan `CMakeLists.txt` untuk daftar lengkap dependencies.

---

**Made with ❤️ by Wahyunugroho99**
