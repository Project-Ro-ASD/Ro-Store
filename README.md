# Ro-Store

![Ro-Store logo](assets/ro-store.svg)

**Ro-ASD grafiksel uygulama mağazası · The graphical application store for Ro-ASD**

[🇹🇷 Türkçe](#türkçe) | [🇬🇧 English](#english)

---

## Türkçe

### Ro-Store nedir?

**Ro-Store**, Ro-ASD için geliştirilen, Qt 6/QML tabanlı bir Linux uygulama mağazasıdır. Ro-ASD deposundaki uygulamaları grafiksel arayüz üzerinden keşfetmeyi, kurmayı, kaldırmayı, güncellemeyi ve çalıştırmayı amaçlar. Paket işlemlerini **DNF5 daemon** ve **D-Bus** üzerinden gerçekleştirir.

### v0.3.1 özellikleri

- Ro-ASD uygulama kataloğu, arama, kategori filtreleri ve uygulama detayları
- RPM uygulamalarını DNF5 üzerinden kurma, kaldırma ve güncelleme altyapısı
- Kurulu uygulamaları çalıştırma
- İşlem kuyruğu, indirme ilerlemesi, hız, tahmini kalan süre ve işlem geçmişi
- Ro-ASD deposunu algılama; eksikse ekleme, devre dışıysa etkinleştirme
- Depo yapılandırma işlemleri için **Polkit** üzerinden yönetici yetkilendirmesi
- Depo hazır değilken kurulum/güncelleme işlemlerini engelleme
- KDE/Plasma renk paletine uyum, uygulama açıkken değişen sistem yazı tipi ve simge temasını takip etme
- Küçük pencere ve ölçekleme uyumu, Tab / Shift+Tab / Enter / Esc klavye erişilebilirliği
- Açık ve koyu temalarda okunabilir düğmeler, odak göstergeleri ve kontrastlı simge yüzeyleri

> **Mevcut kapsam:** v0.3.1, Ro-ASD deposundaki RPM uygulamalarına odaklanır. Genel Fedora uygulama kataloğu ve Flatpak desteği henüz bu sürümün parçası değildir.

### Sistem gereksinimleri

- **Fedora 44**
- **x86_64** veya **aarch64** mimarisi
- DNF5 ve Polkit (RPM kurulumu sırasında gerekli paket bağımlılıkları otomatik olarak çözülür)

Mimariyi kontrol etmek için:

~~~bash
uname -m
~~~

### Ro-Store kurulumu

1. [Ro-Store v0.3.1 sürüm sayfasını](https://github.com/Project-Ro-ASD/Ro-Store/releases/tag/v0.3.1) açın.
2. Bilgisayarınızın mimarisine uygun RPM dosyasını indirin.
3. Dosyayı **Home (ev dizini)** klasörüne koyduysanız aşağıdaki komutu kullanın.

**x86_64 (Intel / AMD):**

~~~bash
cd ~
sudo dnf5 install ./ro-store-0.3.1-1.fc44.x86_64.rpm
~~~

**aarch64 (ARM64):**

~~~bash
cd ~
sudo dnf5 install ./ro-store-0.3.1-1.fc44.aarch64.rpm
~~~

> RPM farklı bir klasördeyse komuttaki dosya yolunu değiştirin. Aynı kurulum komutu, daha eski bir Ro-Store sürümü kuruluysa yükseltmek için de kullanılabilir.

Kurulumu doğrulayın:

~~~bash
rpm -q ro-store
~~~

### Ro-Store'u başlatma

Uygulama menüsünden **Ro-Store**'u açın veya terminalde:

~~~bash
ro-store
~~~

Ro-ASD deposu sistemde yoksa uygulama içerisindeki **Ro-ASD Deposunu Ekle** düğmesini kullanın. Gerektiğinde Polkit yetkilendirmesi istenir. Depo devre dışıysa **Depoyu Etkinleştir** düğmesiyle yeniden etkinleştirilebilir.

### Ro-Store'u kaldırma

~~~bash
sudo dnf5 remove ro-store
~~~

> **Önemli:** Ro-Store'un kaldırılması, daha önce Ro-Store üzerinden yüklediğiniz uygulamaları otomatik olarak kaldırmaz. Uygulamanın sonradan eklediği Ro-ASD depo tanımları ve GPG anahtarları da sistemde kalabilir; bunlar Ro-Store RPM paketinin kendisine ait dosyalar değildir.

### Bağlantılar

- [Son sürümler / Releases](https://github.com/Project-Ro-ASD/Ro-Store/releases)
- [Hata bildirimi ve öneriler / Issues](https://github.com/Project-Ro-ASD/Ro-Store/issues)

---

## English

### What is Ro-Store?

**Ro-Store** is a Qt 6/QML-based graphical application store for Ro-ASD. It provides an interface for discovering, installing, removing, updating, and launching applications available from the Ro-ASD repository. Package transactions use the **DNF5 daemon** over **D-Bus**.

### Features in v0.3.1

- Ro-ASD application catalog, search, category filters, and application details
- RPM installation, removal, and update infrastructure through DNF5
- Launching installed applications
- Transaction queue, download progress, speed, estimated remaining time, and transaction history
- Detection of the Ro-ASD repository, with guided setup and re-enabling when disabled
- Administrator authorization through **Polkit** for repository configuration
- Protection against install/update actions when the repository is not ready
- KDE/Plasma palette integration, live system font and icon-theme updates
- Responsive layouts and keyboard navigation with Tab / Shift+Tab / Enter / Escape
- Accessible action buttons, focus indicators and icon backgrounds across light/dark themes

> **Current scope:** v0.3.1 focuses on RPM applications provided by the Ro-ASD repository. A general Fedora application catalog and Flatpak support are not included yet.

### System requirements

- **Fedora 44**
- **x86_64** or **aarch64** architecture
- DNF5 and Polkit (required RPM dependencies are resolved during installation)

Check your system architecture:

~~~bash
uname -m
~~~

### Install Ro-Store

1. Open the [Ro-Store v0.3.1 release page](https://github.com/Project-Ro-ASD/Ro-Store/releases/tag/v0.3.1).
2. Download the RPM matching your system architecture.
3. If the RPM is in your **Home directory**, run the corresponding command below.

**x86_64 (Intel / AMD):**

~~~bash
cd ~
sudo dnf5 install ./ro-store-0.3.1-1.fc44.x86_64.rpm
~~~

**aarch64 (ARM64):**

~~~bash
cd ~
sudo dnf5 install ./ro-store-0.3.1-1.fc44.aarch64.rpm
~~~

> Adjust the file path if you saved the RPM elsewhere. The same install command can also upgrade an older installed Ro-Store version.

Verify the installation:

~~~bash
rpm -q ro-store
~~~

### Launch Ro-Store

Open **Ro-Store** from the application menu or run:

~~~bash
ro-store
~~~

If the Ro-ASD repository is missing, select **Ro-ASD Deposunu Ekle** (Add Ro-ASD Repository) in the application. Polkit authorization is requested when necessary. If the repository is disabled, use **Depoyu Etkinleştir** (Enable Repository).

> The application interface currently uses Turkish labels; this README provides instructions in both Turkish and English.

### Uninstall Ro-Store

~~~bash
sudo dnf5 remove ro-store
~~~

> **Important:** Removing Ro-Store does not automatically remove applications you previously installed through it. Ro-ASD repository configuration and GPG keys added at runtime may also remain on the system because they are not owned by the Ro-Store RPM package.

### Links

- [Releases](https://github.com/Project-Ro-ASD/Ro-Store/releases)
- [Issues and suggestions](https://github.com/Project-Ro-ASD/Ro-Store/issues)
