# Ro-Store v1.0 user guide / Kullanıcı rehberi

## Türkçe

Ro-Store, Fedora 44 ve Ro-ASD için RPM uygulamalarını, sistem güncellemelerini
ve kullanıcı kapsamındaki Flathub uygulamalarını tek pencereden yönetir.
Fedora 44 üzerinde x86_64 ve aarch64 paketleri yayımlanır. Ro-ASD kabul
testleri tamamlanmadan Ro-ASD desteği doğrulanmış sayılmaz.

### Kurulum ve yükseltme

1. Ro-ASD'nin imzalı Fedora 44 paket kanalını ve güvenilir RPM anahtarını
   yapılandırın. Ana anahtar parmak izi
   `9A66A082D96374DA27D7538FB2186A6F2FD84CB3` olmalıdır.
2. `sudo dnf5 install ro-store` komutunu çalıştırın. Önceki sürüm kuruluysa
   `sudo dnf5 upgrade ro-store` ile güncelleyin. RC'den kararlı sürüme geçiş
   de bu yoldan yapılır.
3. `rpm -q ro-store` ile kurulan sürümü kontrol edin. İmzayı ayrıca görmek
   için kanaldan indirilen RPM'de `rpmkeys --checksig --verbose` çalıştırın.

GitHub'daki Ro-Store sürüm RPM'leri Ro-Repo'nun imzalayıp kanala aldığı
**üretici girdileridir**; doğrudan kurulum için imzalı paket sayılmazlar.

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

1. Configure the signed Ro-ASD Fedora 44 package channel and obtain its RPM
   public key through a trusted source. The primary key fingerprint is
   `9A66A082D96374DA27D7538FB2186A6F2FD84CB3`.
2. Run `sudo dnf5 install ro-store`, or `sudo dnf5 upgrade ro-store` for an
   existing installation. The stable package upgrades a release candidate.
3. Check the installed version with `rpm -q ro-store`. For an independent
   signature check, run `rpmkeys --checksig --verbose` on an RPM downloaded
   from the signed channel.

The RPMs on the Ro-Store GitHub release page are unsigned producer inputs for
Ro-Repo. Install the centrally signed packages from the Ro-ASD channel.

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
