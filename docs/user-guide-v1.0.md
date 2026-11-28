# Ro-Store v1.0 user guide / Kullanıcı rehberi

## Türkçe

Ro-Store, Fedora 44 ve Ro-ASD için RPM uygulamalarını, sistem güncellemelerini
ve kullanıcı kapsamındaki Flathub uygulamalarını tek pencereden yönetir.
Fedora 44 üzerinde x86_64 ve aarch64 paketleri yayımlanır. Ro-ASD kabul
testleri tamamlanmadan Ro-ASD desteği doğrulanmış sayılmaz.

### Kurulum ve yükseltme

1. [Sürümler sayfasından](https://github.com/Project-Ro-ASD/Ro-Store/releases)
   mimarinize uygun `ro-store-1.0.0-1.fc44` RPM dosyasını ve
   `SHA256SUMS.txt` dosyasını aynı klasöre indirin. RC sınaması için
   `ro-store-1.0.0-0.rc1.fc44` dosyasını kullanın.
2. `sha256sum -c SHA256SUMS.txt` ile indirilen dosyayı kontrol edin.
3. İmza anahtarını Ro-ASD'nin güvenilir kanalından edinin; parmak izi
   `9A66A082D96374DA27D7538FB2186A6F2FD84CB3` olmalıdır. Anahtarı
   `sudo rpmkeys --import RPM-GPG-KEY-ro-asd` ile içe aktarın ve
   `rpmkeys --checksig --verbose ./ro-store-*.rpm` çıktısında imzanın
   `OK` olduğunu doğrulayın.
4. `sudo dnf5 install ./ro-store-1.0.0-1.fc44.x86_64.rpm` komutunu çalıştırın.
   aarch64 için dosya adındaki mimariyi değiştirin. Önceki sürüm kuruluysa
   DNF5 yükseltme işlemini yapar. RC'den kararlı sürüme geçiş de aynı yoldur.

İşlemden sonra `rpm -q ro-store` ve `dnf5 history` ile sonucu görün. Sistem
güncellemesi ekranda önce bir DNF5 planı gösterir; kaldırma veya sürüm düşürme
içeren güvenli olmayan planların hazırlanması engellenir. Çevrimdışı güncelleme
hazırlığı tek başına yeniden başlatmaz.

### Sorun giderme

- Katalog boşsa ağ bağlantısını, DNF5 depolarını ve Flathub kaynağını kontrol
  edin; aramayı yeniden deneyin. Önbellekte eski sonuçlar görünebilir.
- Yetki reddedilirse yönetici onayı gerektiren RPM işlemine yeniden başlayın.
  Bilinmeyen bir işlemi uygulama içinde yeniden çalıştırmadan önce DNF5
  geçmişini kontrol edin.
- Yetersiz disk alanı veya başarısız işlem sonrasında geçmiş ve kurtarma
  durumunu inceleyin. Çevrimdışı güncelleme için bilgisayarı zorla kapatmayın.
- Flatpak izin bilgisi yüklenemezse ayrıntı ekranını yeniden açıp bağlantıyı
  kontrol edin; izin özeti görülmeden kurulumu onaylamayın.

**Sınırlar:** Flatpak güncellemeleri ve sistem kapsamındaki Flatpak işlemleri
kapalıdır. Çalışma ortamları, sürücüler, codec'ler ve eklentiler bu yoldan
değiştirilmez. Fedora dağıtım sürümü yükseltmesi ve otomatik güncelleme
zamanlaması bu sürümde yoktur. Ro-Store'u kaldırmak daha önce kurulan
uygulamaları otomatik kaldırmaz.

## English

Ro-Store manages RPM applications, system updates and user-scoped Flathub
applications in one window on Fedora 44 and Ro-ASD. Fedora 44 packages target
x86_64 and aarch64. Ro-ASD support must not be considered validated until
acceptance testing on Ro-ASD hardware is complete.

### Install and upgrade

1. Download the architecture-specific `ro-store-1.0.0-1.fc44` RPM and
   `SHA256SUMS.txt` from the [releases page](https://github.com/Project-Ro-ASD/Ro-Store/releases).
   The candidate package uses `0.rc1.fc44` in its filename.
2. Run `sha256sum -c SHA256SUMS.txt` in the download directory.
3. Obtain the Ro-ASD RPM public key through a trusted Ro-ASD channel. Check
   primary fingerprint `9A66A082D96374DA27D7538FB2186A6F2FD84CB3`,
   import it with `sudo rpmkeys --import RPM-GPG-KEY-ro-asd`, and require an
   `OK` signature from `rpmkeys --checksig --verbose ./ro-store-*.rpm`.
4. Install with `sudo dnf5 install ./ro-store-1.0.0-1.fc44.x86_64.rpm`.
   Substitute `aarch64` on ARM. DNF5 upgrades an existing installation,
   including the release candidate.

Check `rpm -q ro-store` and `dnf5 history` afterward. System updates show a
DNF5 plan before preparation; unsafe plans with removals or downgrades are
blocked. Offline update preparation does not restart the computer on its own.

### Troubleshooting

- If results are empty, check connectivity, DNF5 repositories and the Flathub
  source, then retry. Cached results may be stale.
- If authorization is denied, restart the RPM operation and inspect DNF5
  history before retrying an uncertain transaction.
- After low disk space or a failed transaction, inspect history and recovery
  status. Do not force a power loss during an offline update.
- If Flatpak permissions cannot be read, reopen the details and check the
  connection. Do not approve installation without reviewing permissions.

**Limits:** Flatpak updates and system-scoped Flatpak changes are disabled.
Runtimes, drivers, codecs and extensions are not modified through that path.
Fedora distribution upgrades and automatic update scheduling are not included.
Uninstalling Ro-Store does not automatically remove applications installed
through it.
