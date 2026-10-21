# Ro-Store

![Ro-Store logo](assets/ro-store.svg)

**Ro-ASD grafiksel uygulama mağazası · The graphical application store for Ro-ASD**

[🇹🇷 Türkçe](#türkçe) | [🇬🇧 English](#english)

---

## Türkçe

## v0.7.0 — Flatpak / Flathub (sürüm adayı; henüz yayımlanmadı)

Bu dal Fedora 44 için ayrı bir **Flatpak / Flathub** ekranı ekler: kullanıcı/sistem
kapsamında kaynakları, kurulmuş uygulamaları ve güncelleme adaylarını gösterir;
uygulama keşfi, simge, açıklama, ekran görüntüleri ve uygulama detayları bulunur.
Kullanıcı onayıyla Flathub kullanıcı kaynağını ekleme, uygulama kurma,
kaldırma ve çalıştırma akışları desteklenir.

**Doğrulananlar:** Fedora 44 x86_64'te Flathub kaynağı ekleme ve Curo
Calculator kurma–kaldırma–yeniden kurma–çalıştırma. Durum düğmeleri sayfadan
çıkmadan yenilenir. Onaylanmış işlemlerde Flatpak güvenlik kaydı otomatik
temizlenir. RPM/DNF5 uygulama yolu bağımsızdır.

**Sınırlar:** Uygulama güncellemesi doğrulama korumaları içerir ancak gerçek
güncelleme henüz test edilmemiştir. NVIDIA sürücüleri, Flatpak çalışma
ortamları ve uzantıları bu akışla güncellenmez. Kesinti/yetki reddi/
çoklu örnek kurtarma testleri henüz tamamlanmadığı için **v0.7.0 release
veya mevcut Fedora sistemine RPM yükseltmesi önerilmiyor**. Test planı:
[docs/testing/v0.7-flatpak.md](docs/testing/v0.7-flatpak.md).

### Geliştirme dalını kaynak koddan çalıştırma

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
./build/ro-store
```

Flatpak yüklü değilse Flatpak ekranı bunu açıklar; mevcut RPM işlemleri
ayrıca kullanılabilir. Flatpak ve RPM katalogları henüz tek aramada
birleştirilmemiştir.

## v0.6.0 — Fedora RPM sistem güncellemeleri

Ro-Store v0.6.0, Fedora 44 üzerindeki RPM sistem güncellemelerini DNF5 ile
denetleyip seçilen paketler için tek bir bağımlılık çözümleme planı oluşturur.
Kullanıcı plandaki ekleme, değiştirme ve kaldırma işlemlerini inceleyebilir.
İlk güvenlik sürümünde kaldırma, sürüm düşürme ve tanınmayan işlem içeren
planların hazırlanması engellenir.

Ayrı kullanıcı onayıyla DNF5 çevrimdışı güncelleme dosyaları hazırlanır.
Bu aşama kurulu RPM'leri değiştirmez ve sistemi otomatik yeniden başlatmaz.
Hazırlık iptal edilebilir; uygulama öncesi ayrı yeniden başlatma onayı gerekir.
Belirsiz sonuçlarda güvenlik kaydı korunur ve yeniden başlatma sonrasında
DNF5 geçmişi incelenerek kurtarma kilidi kaldırılır.

**Doğrulama:** Fedora 44 x86_64 üzerinde 7zip ve assimp için 4 işlemlik
plan (0 kaldırma) hazırlanıp başarıyla iptal edildi. Daha sonra aynı
paketler çevrimdışı yeniden başlatmayla güncellendi; DNF5 history 175
`Status: Ok` bildirdi ve kurulu yeni sürümler doğrulandı.
Fedora 44 x86_64/aarch64 RPM derleme, kurulum ve başlatma CI kontrolleri
uygulanır. aarch64 üzerinde gerçek çevrimdışı RPM güncellemesi denenmedi.

**Kapsam:** Fedora 44 RPM güncellemeleri; Flatpak ve Fedora 44→45 dağıtım
sürümü yükseltmesi kapsam dışındadır. Kernel güncelleme adayları gösterilir,
ancak gerçek kernel/NVIDIA çevrimdışı uygulaması bu testte doğrulanmadı.
DNF5 durum ve işlem geçmişi, kurulumu doğrulamak için temel referanstır.

### v0.6.0 kurulum

[Ro-Store sürümlerinden](https://github.com/Project-Ro-ASD/Ro-Store/releases)
mimarinize uygun `ro-store-0.6.0-1.fc44.x86_64.rpm` veya
`ro-store-0.6.0-1.fc44.aarch64.rpm` dosyasını indirin (sürüm yayımlandıktan
sonra). Dosyanın bulunduğu klasörde:

~~~bash
sudo dnf5 install ./ro-store-0.6.0-1.fc44.x86_64.rpm
~~~

ARM64 kullanıyorsanız komuttaki RPM adını `aarch64` paketiyle değiştirin.


## v0.5.0 — AppStream ile Fedora uygulama keşfi

Ro-Store v0.5.0, Ro-ASD uygulama kataloğunu koruyarak Fedora RPM kataloğunu
AppStream uygulama metadatasıyla zenginleştirir. Uygulama adı, açıklama,
kategori, simge ve varsa ekran görüntüleri gösterilir. Ekran görüntüleri
büyütülebilir, yatay kaydırılabilir; görsel sunucusu uygulama içinden erişimi
engellerse bağlantı varsayılan tarayıcıda açılabilir.

Katalog yalnızca örnek paketlere bağlı kalmaz: AppStream'deki masaüstü
uygulamaları, etkin DNF5 depolarındaki gerçek RPM paketleriyle eşleştirilerek
keşfedilir. RPM adına ek olarak uygulamanın görünen adı ve özet açıklamasıyla
arama; kaynak ve kategori filtreleri; 48'lik sayfalama desteklenir. İşlem
yürütme ve paket kimliği DNF5 üzerinden devam eder.

### v0.5.0 doğrulama

1. Fedora 44 üzerinde **Fedora RPM Kataloğu**'na girin. AppStream destekli
   uygulamaların başlangıçta listelendiğini doğrulayın.
2. `firefox`, `krita`, `lazpaint` ve uygulama görünen adlarıyla arama yapın.
3. **Kaynak** ve **Kategori** filtrelerini ayrı ayrı ve birlikte deneyin;
   **Tümü** seçimiyle sonuçların geri geldiğini doğrulayın.
4. **Daha fazla göster** düğmesiyle sayfalama ve uygulama detaylarını kontrol edin.
5. Simgeleri, açıklamaları, ekran görüntüsü önizlemesini ve yatay kaydırmayı
   deneyin. Ulaşılamayan görseller için tarayıcıda açma seçeneğini test edin.
6. 400×500 / 800×600 pencerelerde, açık/koyu KDE temalarında ve klavye ile
   gezinmede kullanılabilirliği kontrol edin.

**Kapsam:** AppStreamQt bulunamazsa önceki RPM katalog davranışına geri
dönülür. Paket yükleme/kaldırma işlemleri DNF5 ile yapılır. Flatpak desteği
bu sürümde yoktur. Ro-ASD uygulamaları kendi kataloğunda kalır.

### Ro-Store nedir?

**Ro-Store**, Ro-ASD için geliştirilen, Qt 6/QML tabanlı bir Linux uygulama mağazasıdır. Ro-ASD deposundaki uygulamaları grafiksel arayüz üzerinden keşfetmeyi, kurmayı, kaldırmayı, güncellemeyi ve çalıştırmayı amaçlar. Paket işlemlerini **DNF5 daemon** ve **D-Bus** üzerinden gerçekleştirir.

### v0.5.0 özellikleri

- Ro-ASD uygulama kataloğu, arama, kategori filtreleri ve uygulama detayları
- Fedora AppStream uygulama keşfi; simge, açıklama, ekran görüntüleri ve önizleme
- Uygulama adına göre arama, Fedora deposu ve AppStream kategori filtreleri
- RPM uygulamalarını DNF5 üzerinden kurma, kaldırma ve güncelleme altyapısı
- Kurulu uygulamaları çalıştırma
- İşlem kuyruğu, indirme ilerlemesi, hız, tahmini kalan süre ve işlem geçmişi
- Ro-ASD deposunu algılama; eksikse ekleme, devre dışıysa etkinleştirme
- Depo yapılandırma işlemleri için **Polkit** üzerinden yönetici yetkilendirmesi
- Depo hazır değilken kurulum/güncelleme işlemlerini engelleme
- KDE/Plasma renk paletine uyum, uygulama açıkken değişen sistem yazı tipi ve simge temasını takip etme
- Küçük pencere ve ölçekleme uyumu, Tab / Shift+Tab / Enter / Esc klavye erişilebilirliği
- Açık ve koyu temalarda okunabilir düğmeler, odak göstergeleri ve kontrastlı simge yüzeyleri

> **Mevcut kapsam:** v0.5.0 Fedora RPM uygulama keşfini AppStream ile zenginleştirir. Flatpak desteği henüz bulunmaz.

### Sistem gereksinimleri

- **Fedora 44**
- **x86_64** veya **aarch64** mimarisi
- DNF5 ve Polkit (RPM kurulumu sırasında gerekli paket bağımlılıkları otomatik olarak çözülür)

Mimariyi kontrol etmek için:

~~~bash
uname -m
~~~

### Ro-Store kurulumu

1. [Ro-Store v0.5.0 sürüm sayfasını](https://github.com/Project-Ro-ASD/Ro-Store/releases/tag/v0.5.0) açın.
2. Bilgisayarınızın mimarisine uygun RPM dosyasını indirin.
3. Dosyayı **Home (ev dizini)** klasörüne koyduysanız aşağıdaki komutu kullanın.

**x86_64 (Intel / AMD):**

~~~bash
cd ~
sudo dnf5 install ./ro-store-0.5.0-1.fc44.x86_64.rpm
~~~

**aarch64 (ARM64):**

~~~bash
cd ~
sudo dnf5 install ./ro-store-0.5.0-1.fc44.aarch64.rpm
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

### Features in v0.5.0

- Ro-ASD application catalog, search, category filters, and application details
- AppStream-enriched Fedora RPM discovery with icons, descriptions, screenshots and previews
- Search by application display name, repository and category filters, paginated discovery
- RPM installation, removal, and update infrastructure through DNF5
- Launching installed applications
- Transaction queue, download progress, speed, estimated remaining time, and transaction history
- Detection of the Ro-ASD repository, with guided setup and re-enabling when disabled
- Administrator authorization through **Polkit** for repository configuration
- Protection against install/update actions when the repository is not ready
- KDE/Plasma palette integration, live system font and icon-theme updates
- Responsive layouts and keyboard navigation with Tab / Shift+Tab / Enter / Escape
- Accessible action buttons, focus indicators and icon backgrounds across light/dark themes

> **Current scope:** v0.5.0 discovers Fedora RPM desktop applications with optional AppStream enrichment while preserving the separate Ro-ASD catalog. Flatpak support is not included.

### System requirements

- **Fedora 44**
- **x86_64** or **aarch64** architecture
- DNF5 and Polkit (required RPM dependencies are resolved during installation)

Check your system architecture:

~~~bash
uname -m
~~~

### Install Ro-Store

1. Open the [Ro-Store v0.5.0 release page](https://github.com/Project-Ro-ASD/Ro-Store/releases/tag/v0.5.0).
2. Download the RPM matching your system architecture.
3. If the RPM is in your **Home directory**, run the corresponding command below.

**x86_64 (Intel / AMD):**

~~~bash
cd ~
sudo dnf5 install ./ro-store-0.5.0-1.fc44.x86_64.rpm
~~~

**aarch64 (ARM64):**

~~~bash
cd ~
sudo dnf5 install ./ro-store-0.5.0-1.fc44.aarch64.rpm
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
