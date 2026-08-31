#include "ota_update.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sntp.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include "firmware_version.h"

namespace {

constexpr char kLatestReleaseApiUrl[] =
    "https://api.github.com/repos/M1XZG/cyd-desk-dashboard/releases/latest";
constexpr char kFirmwareAssetUrlPrefix[] =
    "https://github.com/M1XZG/cyd-desk-dashboard/releases/download/";
constexpr size_t kMaximumReleaseResponseBytes = 128U * 1024U;
constexpr uint32_t kNetworkLockTimeoutMilliseconds = 15000;
constexpr uint32_t kDownloadStallTimeoutMilliseconds = 15000;

constexpr char kGitHubCaBundle[] = R"pem(
-----BEGIN CERTIFICATE-----
MIIDXzCCAuagAwIBAgIQNuBZ7YiN1Xrt1XC2cn+b2jAKBggqhkjOPQQDAzBfMQsw
CQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1T
ZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBFNDYwHhcN
MjEwMzIyMDAwMDAwWhcNMzYwMzIxMjM1OTU5WjBgMQswCQYDVQQGEwJHQjEYMBYG
A1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5TZWN0aWdvIFB1YmxpYyBT
ZXJ2ZXIgQXV0aGVudGljYXRpb24gQ0EgRFYgRTM2MFkwEwYHKoZIzj0CAQYIKoZI
zj0DAQcDQgAEaKGnbAUnBYljHDmn/yUhxe3TLxKYuyzc9VXoSaCEV5F73Fhfa/Si
/RMsmwTFW3R9s7J6JpYZFmu4do3vk/Vgl6OCAYEwggF9MB8GA1UdIwQYMBaAFNEi
2kxZ8UtfJjiqndbu6w3D+6lhMB0GA1UdDgQWBBQXmagEwW/kLXCoChA9A9PpGrgm
YzAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHSUEFjAU
BggrBgEFBQcDAQYIKwYBBQUHAwIwGwYDVR0gBBQwEjAGBgRVHSAAMAgGBmeBDAEC
ATBUBgNVHR8ETTBLMEmgR6BFhkNodHRwOi8vY3JsLnNlY3RpZ28uY29tL1NlY3Rp
Z29QdWJsaWNTZXJ2ZXJBdXRoZW50aWNhdGlvblJvb3RFNDYuY3JsMIGEBggrBgEF
BQcBAQR4MHYwTwYIKwYBBQUHMAKGQ2h0dHA6Ly9jcnQuc2VjdGlnby5jb20vU2Vj
dGlnb1B1YmxpY1NlcnZlckF1dGhlbnRpY2F0aW9uUm9vdEU0Ni5wN2MwIwYIKwYB
BQUHMAGGF2h0dHA6Ly9vY3NwLnNlY3RpZ28uY29tMAoGCCqGSM49BAMDA2cAMGQC
MFsKnBQDh64l+v+aUYWjDCJKQMxHUUGmcwAYDIjJ9pbRYItMCIx5xu0oUb6sIfTX
qQIwPddcsDE4KdeLu1hJdpHgdLvsHAK3vygyLGujMU9xBJCDackRT93VHEE0gppg
NqdV
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIE2zCCAsOgAwIBAgIRAKICU/FfJpHAXcHOE7m8yk4wDQYJKoZIhvcNAQELBQAw
LjELMAkGA1UEBhMCVVMxDTALBgNVBAoTBElTUkcxEDAOBgNVBAMTB1Jvb3QgWVIw
HhcNMjUwOTAzMDAwMDAwWhcNMjgwOTAyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEW
MBQGA1UEChMNTGV0J3MgRW5jcnlwdDEMMAoGA1UEAxMDWVIxMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAoVi8X2xCYgMXvJxNPKp/oF13UMgmPABB07VC
LNDtoXmt9luEZNJSBV10VyT1Pz6LD8Zq1d2gc43WNl1AdRrj4sEnazbOiz0nPpmG
Bp2hui49oZtDIY6wdKeZAi5BbNU20CH6RSBBMLSQ9cXrH8dxdv4PAJ45ssGML68U
SE3BsjC2a6cAN9L5CgXVIQi5tfNiTPoFZZ3S0OlXqLmmtdV95udWAb5b6e/F49Di
CsH0Y00Ag72BVIb1hzynmKe+X0mERBTtsb3BwmpV9ipeBjMLoR/D9cHxHQCWoi5l
TmXwY015J5rGelz1nZjJuxc2kioaX29XJBnhMkP531rSdG5uMwIDAQABo4HuMIHr
MA4GA1UdDwEB/wQEAwIBhjATBgNVHSUEDDAKBggrBgEFBQcDATASBgNVHRMBAf8E
CDAGAQH/AgEAMB0GA1UdDgQWBBQfLzW+RhSCzUCxrnksVXj699Ro+zAfBgNVHSME
GDAWgBTe51tg0CJtQCh9Pw0B/qS1UrRRlDAyBggrBgEFBQcBAQQmMCQwIgYIKwYB
BQUHMAKGFmh0dHA6Ly95ci5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwB
AgEwJwYDVR0fBCAwHjAcoBqgGIYWaHR0cDovL3lyLmMubGVuY3Iub3JnLzANBgkq
hkiG9w0BAQsFAAOCAgEA0+zvMq3kHig1ddTmmm+RibTr9/RpX7k4buanMMRqbV/y
IvP82zAHN3mvaw+cASuVsdpd0ikjhr4hnhJQLQOzOp2ccKrsdGOAgo0vddeISFAq
EWEV4lmUM3vFF796up+bSgmJ1u6RupDCMxDgF8M3eLvGuj6L0lu3zkQ0KuQLnKxL
tB0oQqn1Idg5CuuGpMvQzk29Pa3D/qHurc0EIM9SxukQuJqq63lxsYyRQFU8yMBO
hq1w5LbfaWNRrz1uklOfI/pYkAb2E2MTZrAMQkBIE2S8Jt1F8gRc96o/xOsrgvSk
a84AisX6xq1lz1Z7jGvrnXc4TMcjxZTjiTaihcYI1JIXZiLtEMSCa5l3cu8YWd6z
dLRQlqRdclVjuQfNHawRJ6GWlkK0QJosivTKwdBw3KxEtzGo8yMHERbsy57gP1UX
HOMcmZYQC0gtyR3SxfenIM/MxC3Ia2Ypab/kQ/CTnlIn2KQ5JUC6NYrGCbhFN9bp
5lKJStEwCUnLpntcrXk5XVDCNv/5RyWpRThkGOV7GetKkQ0qAY8hCzWK6oqnAhDZ
cjlYVdWfqOw3DIOX6EDNBgAqHarRVxyF9QZdOaXSyPJ0ueD2BYJEBgaCGQ8rAaU/
Qc123V5LTXDZW4CcsPBDyhy4v+c8hClAyw/IkJlfBqxB9D+/wvIMHgECZ4ptP6o=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIDRjCCAsugAwIBAgIQGp6v7G3o4ZtcGTFBto2Q3TAKBggqhkjOPQQDAzCBiDEL
MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl
eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT
JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMjEwMzIy
MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjBfMQswCQYDVQQGEwJHQjEYMBYGA1UEChMP
U2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIg
QXV0aGVudGljYXRpb24gUm9vdCBFNDYwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAR2
+pmpbiDt+dd34wc7qNs9Xzjoq1WmVk/WSOrsfy2qw7LFeeyZYX8QeccCWvkEN/U0
NSt3zn8gj1KjAIns1aeibVvjS5KToID1AZTc8GgHHs3u/iVStSBDHBv+6xnOQ6Oj
ggEgMIIBHDAfBgNVHSMEGDAWgBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAdBgNVHQ4E
FgQU0SLaTFnxS18mOKqd1u7rDcP7qWEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB
/wQFMAMBAf8wHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBEGA1UdIAQK
MAgwBgYEVR0gADBQBgNVHR8ESTBHMEWgQ6BBhj9odHRwOi8vY3JsLnVzZXJ0cnVz
dC5jb20vVVNFUlRydXN0RUNDQ2VydGlmaWNhdGlvbkF1dGhvcml0eS5jcmwwNQYI
KwYBBQUHAQEEKTAnMCUGCCsGAQUFBzABhhlodHRwOi8vb2NzcC51c2VydHJ1c3Qu
Y29tMAoGCCqGSM49BAMDA2kAMGYCMQCMCyBit99vX2ba6xEkDe+YO7vC0twjbkv9
PKpqGGuZ61JZryjFsp+DFpEclCVy4noCMQCwvZDXD/m2Ko1HA5Bkmz7YQOFAiNDD
49IWa2wdT7R3DtODaSXH/BiXv8fwB9su4tU=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
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
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)pem";

constexpr char kGithubRootCa[] = R"pem(
-----BEGIN CERTIFICATE-----
MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL
MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl
eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT
JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx
MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT
Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg
VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm
aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo
I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng
o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G
A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD
VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB
zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW
RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=
-----END CERTIFICATE-----
)pem";

constexpr char kGithubIssuer[] = R"pem(
-----BEGIN CERTIFICATE-----
MIIDXzCCAuagAwIBAgIQNuBZ7YiN1Xrt1XC2cn+b2jAKBggqhkjOPQQDAzBfMQsw
CQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1T
ZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBFNDYwHhcN
MjEwMzIyMDAwMDAwWhcNMzYwMzIxMjM1OTU5WjBgMQswCQYDVQQGEwJHQjEYMBYG
A1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5TZWN0aWdvIFB1YmxpYyBT
ZXJ2ZXIgQXV0aGVudGljYXRpb24gQ0EgRFYgRTM2MFkwEwYHKoZIzj0CAQYIKoZI
zj0DAQcDQgAEaKGnbAUnBYljHDmn/yUhxe3TLxKYuyzc9VXoSaCEV5F73Fhfa/Si
/RMsmwTFW3R9s7J6JpYZFmu4do3vk/Vgl6OCAYEwggF9MB8GA1UdIwQYMBaAFNEi
2kxZ8UtfJjiqndbu6w3D+6lhMB0GA1UdDgQWBBQXmagEwW/kLXCoChA9A9PpGrgm
YzAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHSUEFjAU
BggrBgEFBQcDAQYIKwYBBQUHAwIwGwYDVR0gBBQwEjAGBgRVHSAAMAgGBmeBDAEC
ATBUBgNVHR8ETTBLMEmgR6BFhkNodHRwOi8vY3JsLnNlY3RpZ28uY29tL1NlY3Rp
Z29QdWJsaWNTZXJ2ZXJBdXRoZW50aWNhdGlvblJvb3RFNDYuY3JsMIGEBggrBgEF
BQcBAQR4MHYwTwYIKwYBBQUHMAKGQ2h0dHA6Ly9jcnQuc2VjdGlnby5jb20vU2Vj
dGlnb1B1YmxpY1NlcnZlckF1dGhlbnRpY2F0aW9uUm9vdEU0Ni5wN2MwIwYIKwYB
BQUHMAGGF2h0dHA6Ly9vY3NwLnNlY3RpZ28uY29tMAoGCCqGSM49BAMDA2cAMGQC
MFsKnBQDh64l+v+aUYWjDCJKQMxHUUGmcwAYDIjJ9pbRYItMCIx5xu0oUb6sIfTX
qQIwPddcsDE4KdeLu1hJdpHgdLvsHAK3vygyLGujMU9xBJCDackRT93VHEE0gppg
NqdV
-----END CERTIFICATE-----
)pem";

constexpr char kReleaseAssetsIssuer[] = R"pem(
-----BEGIN CERTIFICATE-----
MIIE2zCCAsOgAwIBAgIRAKICU/FfJpHAXcHOE7m8yk4wDQYJKoZIhvcNAQELBQAw
LjELMAkGA1UEBhMCVVMxDTALBgNVBAoTBElTUkcxEDAOBgNVBAMTB1Jvb3QgWVIw
HhcNMjUwOTAzMDAwMDAwWhcNMjgwOTAyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEW
MBQGA1UEChMNTGV0J3MgRW5jcnlwdDEMMAoGA1UEAxMDWVIxMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAoVi8X2xCYgMXvJxNPKp/oF13UMgmPABB07VC
LNDtoXmt9luEZNJSBV10VyT1Pz6LD8Zq1d2gc43WNl1AdRrj4sEnazbOiz0nPpmG
Bp2hui49oZtDIY6wdKeZAi5BbNU20CH6RSBBMLSQ9cXrH8dxdv4PAJ45ssGML68U
SE3BsjC2a6cAN9L5CgXVIQi5tfNiTPoFZZ3S0OlXqLmmtdV95udWAb5b6e/F49Di
CsH0Y00Ag72BVIb1hzynmKe+X0mERBTtsb3BwmpV9ipeBjMLoR/D9cHxHQCWoi5l
TmXwY015J5rGelz1nZjJuxc2kioaX29XJBnhMkP531rSdG5uMwIDAQABo4HuMIHr
MA4GA1UdDwEB/wQEAwIBhjATBgNVHSUEDDAKBggrBgEFBQcDATASBgNVHRMBAf8E
CDAGAQH/AgEAMB0GA1UdDgQWBBQfLzW+RhSCzUCxrnksVXj699Ro+zAfBgNVHSME
GDAWgBTe51tg0CJtQCh9Pw0B/qS1UrRRlDAyBggrBgEFBQcBAQQmMCQwIgYIKwYB
BQUHMAKGFmh0dHA6Ly95ci5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwB
AgEwJwYDVR0fBCAwHjAcoBqgGIYWaHR0cDovL3lyLmMubGVuY3Iub3JnLzANBgkq
hkiG9w0BAQsFAAOCAgEA0+zvMq3kHig1ddTmmm+RibTr9/RpX7k4buanMMRqbV/y
IvP82zAHN3mvaw+cASuVsdpd0ikjhr4hnhJQLQOzOp2ccKrsdGOAgo0vddeISFAq
EWEV4lmUM3vFF796up+bSgmJ1u6RupDCMxDgF8M3eLvGuj6L0lu3zkQ0KuQLnKxL
tB0oQqn1Idg5CuuGpMvQzk29Pa3D/qHurc0EIM9SxukQuJqq63lxsYyRQFU8yMBO
hq1w5LbfaWNRrz1uklOfI/pYkAb2E2MTZrAMQkBIE2S8Jt1F8gRc96o/xOsrgvSk
a84AisX6xq1lz1Z7jGvrnXc4TMcjxZTjiTaihcYI1JIXZiLtEMSCa5l3cu8YWd6z
dLRQlqRdclVjuQfNHawRJ6GWlkK0QJosivTKwdBw3KxEtzGo8yMHERbsy57gP1UX
HOMcmZYQC0gtyR3SxfenIM/MxC3Ia2Ypab/kQ/CTnlIn2KQ5JUC6NYrGCbhFN9bp
5lKJStEwCUnLpntcrXk5XVDCNv/5RyWpRThkGOV7GetKkQ0qAY8hCzWK6oqnAhDZ
cjlYVdWfqOw3DIOX6EDNBgAqHarRVxyF9QZdOaXSyPJ0ueD2BYJEBgaCGQ8rAaU/
Qc123V5LTXDZW4CcsPBDyhy4v+c8hClAyw/IkJlfBqxB9D+/wvIMHgECZ4ptP6o=
-----END CERTIFICATE-----
)pem";

enum class OtaJob : uint8_t {
  check,
  install,
};

struct ReleaseManifest {
  char version[25] = {};
  char sha256[65] = {};
  char firmwareUrl[192] = {};
  uint32_t size = 0;
  bool valid = false;
};

SemaphoreHandle_t statusMutex = nullptr;
SemaphoreHandle_t sharedNetworkMutex = nullptr;
QueueHandle_t jobQueue = nullptr;
OtaStatus status;
ReleaseManifest manifest;
bool jobBusy = false;

class ScopedNetworkLock {
 public:
  ScopedNetworkLock()
      : locked_(
            sharedNetworkMutex == nullptr ||
            xSemaphoreTake(
                sharedNetworkMutex,
                pdMS_TO_TICKS(kNetworkLockTimeoutMilliseconds)) == pdTRUE) {}

  ~ScopedNetworkLock() {
    if (locked_ && sharedNetworkMutex != nullptr) {
      xSemaphoreGive(sharedNetworkMutex);
    }
  }

  explicit operator bool() const {
    return locked_;
  }

 private:
  bool locked_;
};

void setStatus(
    OtaState state,
    const char* error = nullptr,
    uint32_t downloadedBytes = 0) {
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  status.state = state;
  status.downloadedBytes = downloadedBytes;
  strlcpy(status.error, error == nullptr ? "" : error, sizeof(status.error));
  xSemaphoreGive(statusMutex);
}

bool ensureClock(char* error, size_t errorSize) {
  if (time(nullptr) > 1700000000) {
    return true;
  }
  if (!esp_sntp_enabled()) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
  }
  const uint32_t deadline = millis() + 8000;
  while (time(nullptr) <= 1700000000 &&
         static_cast<int32_t>(millis() - deadline) < 0) {
    delay(100);
  }
  if (time(nullptr) <= 1700000000) {
    strlcpy(error, "Could not synchronize time for HTTPS", errorSize);
    return false;
  }
  return true;
}

bool validReleaseVersion(const char* value) {
  const size_t length = strlen(value);
  if (length < 2 || length >= sizeof(manifest.version) || value[0] != 'v') {
    return false;
  }
  int dots = 0;
  bool digitRequired = true;
  for (size_t index = 1; index < length; ++index) {
    const char character = value[index];
    if (character == '.') {
      if (digitRequired || dots >= 2) {
        return false;
      }
      ++dots;
      digitRequired = true;
    } else if (isdigit(static_cast<unsigned char>(character))) {
      digitRequired = false;
    } else {
      return false;
    }
  }
  return dots == 2 && !digitRequired;
}

bool validSha256(const char* value) {
  if (strlen(value) != 64) {
    return false;
  }
  for (size_t index = 0; index < 64; ++index) {
    if (!isxdigit(static_cast<unsigned char>(value[index]))) {
      return false;
    }
  }
  return true;
}

int compareVersions(const char* left, const char* right) {
  const char* leftCursor = left[0] == 'v' ? left + 1 : left;
  const char* rightCursor = right[0] == 'v' ? right + 1 : right;
  for (int component = 0; component < 3; ++component) {
    char* leftEnd = nullptr;
    char* rightEnd = nullptr;
    const unsigned long leftValue = strtoul(leftCursor, &leftEnd, 10);
    const unsigned long rightValue = strtoul(rightCursor, &rightEnd, 10);
    if (leftEnd == leftCursor || rightEnd == rightCursor) {
      return strcmp(left, right);
    }
    if (leftValue != rightValue) {
      return leftValue < rightValue ? -1 : 1;
    }
    leftCursor = *leftEnd == '.' ? leftEnd + 1 : leftEnd;
    rightCursor = *rightEnd == '.' ? rightEnd + 1 : rightEnd;
  }
  const bool leftPrerelease = *leftCursor == '-';
  const bool rightPrerelease = *rightCursor == '-';
  if (leftPrerelease != rightPrerelease) {
    return leftPrerelease ? -1 : 1;
  }
  return strcmp(leftCursor, rightCursor);
}

void configureRequest(HTTPClient& request) {
  request.setConnectTimeout(8000);
  request.setTimeout(15000);
  request.useHTTP10(true);
}

bool fetchManifest(char* error, size_t errorSize) {
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(error, "Wi-Fi is not connected", errorSize);
    return false;
  }
  if (!ensureClock(error, errorSize)) {
    return false;
  }

  ScopedNetworkLock networkLock;
  if (!networkLock) {
    strlcpy(error, "Another network request is still running", errorSize);
    return false;
  }

  WiFiClientSecure client;
  // Arduino ESP32 2.0.x can fail to build GitHub's cross-signed ECC chain.
  // Trust the long-lived GitHub API issuing CA directly instead.
  client.setCACert(kGithubIssuer);
  client.setHandshakeTimeout(8);
  Serial.printf(
      "[OTA] API TLS heap=%u largest=%u\n",
      ESP.getFreeHeap(),
      ESP.getMaxAllocHeap());
  HTTPClient request;
  configureRequest(request);
  if (!request.begin(client, kLatestReleaseApiUrl)) {
    strlcpy(error, "Could not initialize the GitHub API request", errorSize);
    return false;
  }
  request.addHeader(
      "User-Agent",
      "CYD-Desk-Dashboard/" + String(kFirmwareVersion));
  request.addHeader("Accept", "application/vnd.github+json");
  request.addHeader("X-GitHub-Api-Version", "2022-11-28");
  const int httpStatus = request.GET();
  if (httpStatus != HTTP_CODE_OK) {
    snprintf(
        error,
        errorSize,
        "GitHub API HTTP %d: %s",
        httpStatus,
        HTTPClient::errorToString(httpStatus).c_str());
    request.end();
    return false;
  }
  const int responseSize = request.getSize();
  if (responseSize > static_cast<int>(kMaximumReleaseResponseBytes)) {
    strlcpy(error, "The GitHub release response is too large", errorSize);
    request.end();
    return false;
  }

  JsonDocument filter;
  filter["tag_name"] = true;
  JsonObject assetFilter = filter["assets"].add<JsonObject>();
  assetFilter["name"] = true;
  assetFilter["size"] = true;
  assetFilter["digest"] = true;
  assetFilter["browser_download_url"] = true;
  JsonDocument document;
  const DeserializationError jsonError =
      deserializeJson(
          document,
          request.getStream(),
          DeserializationOption::Filter(filter));
  request.end();
  if (jsonError && jsonError != DeserializationError::IncompleteInput) {
    snprintf(error, errorSize, "GitHub release JSON: %s", jsonError.c_str());
    return false;
  }

  const char* version = document["tag_name"] | "";
  JsonObjectConst firmwareAsset;
  for (JsonObjectConst asset : document["assets"].as<JsonArrayConst>()) {
    if (strcmp(asset["name"] | "", "firmware.bin") == 0) {
      firmwareAsset = asset;
      break;
    }
  }
  const char* digest = firmwareAsset["digest"] | "";
  const char* firmwareUrl = firmwareAsset["browser_download_url"] | "";
  const uint32_t size = firmwareAsset["size"] | 0;
  const char* sha256 =
      strncmp(digest, "sha256:", 7) == 0 ? digest + 7 : "";
  if (!validReleaseVersion(version) || !validSha256(sha256) ||
      strlen(firmwareUrl) == 0 ||
      strlen(firmwareUrl) >= sizeof(manifest.firmwareUrl) ||
      strncmp(
          firmwareUrl,
          kFirmwareAssetUrlPrefix,
          strlen(kFirmwareAssetUrlPrefix)) != 0 ||
      size == 0 || size > ESP.getFreeSketchSpace()) {
    strlcpy(
        error,
        jsonError == DeserializationError::IncompleteInput
            ? "The GitHub release response ended before the firmware metadata"
            : "The GitHub firmware asset metadata is invalid",
        errorSize);
    return false;
  }

  strlcpy(manifest.version, version, sizeof(manifest.version));
  strlcpy(manifest.sha256, sha256, sizeof(manifest.sha256));
  strlcpy(
      manifest.firmwareUrl,
      firmwareUrl,
      sizeof(manifest.firmwareUrl));
  manifest.size = size;
  manifest.valid = true;

  xSemaphoreTake(statusMutex, portMAX_DELAY);
  strlcpy(status.latestVersion, manifest.version, sizeof(status.latestVersion));
  status.expectedBytes = manifest.size;
  status.downloadedBytes = 0;
  const int comparison = compareVersions(kFirmwareVersion, manifest.version);
  status.canInstall = comparison <= 0;
  status.reinstall = comparison == 0;
  status.state =
      comparison < 0 ? OtaState::updateAvailable : OtaState::upToDate;
  status.error[0] = '\0';
  xSemaphoreGive(statusMutex);
  return true;
}

void bytesToHex(const uint8_t* bytes, size_t length, char* output) {
  static constexpr char kHex[] = "0123456789abcdef";
  for (size_t index = 0; index < length; ++index) {
    output[index * 2] = kHex[bytes[index] >> 4];
    output[index * 2 + 1] = kHex[bytes[index] & 0x0F];
  }
  output[length * 2] = '\0';
}

bool installRelease(char* error, size_t errorSize) {
  if (!manifest.valid) {
    strlcpy(error, "Check for updates before installing", errorSize);
    return false;
  }
  if (compareVersions(kFirmwareVersion, manifest.version) > 0) {
    strlcpy(error, "Downgrading through OTA is not allowed", errorSize);
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(error, "Wi-Fi is not connected", errorSize);
    return false;
  }
  if (!ensureClock(error, errorSize)) {
    return false;
  }

  ScopedNetworkLock networkLock;
  if (!networkLock) {
    strlcpy(error, "Another network request is still running", errorSize);
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(8);
  HTTPClient request;
  configureRequest(request);
  request.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!request.begin(client, manifest.firmwareUrl)) {
    strlcpy(error, "Could not initialize the firmware download", errorSize);
    return false;
  }
  request.addHeader(
      "User-Agent",
      "CYD-Desk-Dashboard/" + String(kFirmwareVersion));
  const int httpStatus = request.GET();
  if (httpStatus != HTTP_CODE_OK) {
    snprintf(error, errorSize, "Firmware download returned HTTP %d", httpStatus);
    request.end();
    return false;
  }
  const int contentLength = request.getSize();
  if (contentLength > 0 &&
      static_cast<uint32_t>(contentLength) != manifest.size) {
    strlcpy(error, "Firmware size does not match the release manifest", errorSize);
    request.end();
    return false;
  }
  if (!Update.begin(manifest.size, U_FLASH)) {
    snprintf(error, errorSize, "Could not prepare OTA: %s", Update.errorString());
    request.end();
    return false;
  }

  mbedtls_sha256_context shaContext;
  mbedtls_sha256_init(&shaContext);
  if (mbedtls_sha256_starts_ret(&shaContext, 0) != 0) {
    strlcpy(error, "Could not initialize SHA-256 verification", errorSize);
    Update.abort();
    request.end();
    mbedtls_sha256_free(&shaContext);
    return false;
  }

  WiFiClient* stream = request.getStreamPtr();
  uint8_t buffer[4096];
  uint32_t total = 0;
  uint32_t lastDataAt = millis();
  bool failed = false;
  while (total < manifest.size) {
    const size_t available = stream->available();
    if (available == 0) {
      if (!request.connected() ||
          millis() - lastDataAt >= kDownloadStallTimeoutMilliseconds) {
        strlcpy(error, "Firmware download ended before it was complete", errorSize);
        failed = true;
        break;
      }
      delay(10);
      continue;
    }

    const size_t toRead =
        min(sizeof(buffer), min(available, static_cast<size_t>(manifest.size - total)));
    const int count = stream->read(buffer, toRead);
    if (count <= 0) {
      delay(1);
      continue;
    }
    lastDataAt = millis();
    if (mbedtls_sha256_update_ret(&shaContext, buffer, count) != 0 ||
        Update.write(buffer, count) != static_cast<size_t>(count)) {
      snprintf(error, errorSize, "Could not write OTA: %s", Update.errorString());
      failed = true;
      break;
    }
    total += count;
    setStatus(OtaState::downloading, nullptr, total);
    delay(1);
  }
  request.end();

  uint8_t digest[32];
  char digestHex[65];
  if (!failed &&
      mbedtls_sha256_finish_ret(&shaContext, digest) == 0) {
    bytesToHex(digest, sizeof(digest), digestHex);
    if (strcasecmp(digestHex, manifest.sha256) != 0) {
      strlcpy(error, "Firmware SHA-256 verification failed", errorSize);
      failed = true;
    }
  } else if (!failed) {
    strlcpy(error, "Could not finish SHA-256 verification", errorSize);
    failed = true;
  }
  mbedtls_sha256_free(&shaContext);

  if (failed || total != manifest.size) {
    Update.abort();
    return false;
  }
  if (!Update.end()) {
    snprintf(error, errorSize, "Could not activate OTA: %s", Update.errorString());
    return false;
  }
  return true;
}

void worker(void*) {
  OtaJob job;
  while (true) {
    if (xQueueReceive(jobQueue, &job, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    char error[129] = {};
    bool successful = false;
    if (job == OtaJob::check) {
      successful = fetchManifest(error, sizeof(error));
    } else {
      setStatus(OtaState::downloading);
      successful = installRelease(error, sizeof(error));
      if (successful) {
        setStatus(OtaState::readyToRestart, nullptr, manifest.size);
      }
    }
    if (!successful) {
      setStatus(OtaState::error, error);
    }

    xSemaphoreTake(statusMutex, portMAX_DELAY);
    jobBusy = false;
    xSemaphoreGive(statusMutex);
  }
}

bool queueJob(OtaJob job) {
  if (jobQueue == nullptr || statusMutex == nullptr) {
    return false;
  }
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  if (jobBusy) {
    xSemaphoreGive(statusMutex);
    return false;
  }
  if (job == OtaJob::install &&
      (!manifest.valid || !status.canInstall)) {
    xSemaphoreGive(statusMutex);
    return false;
  }
  jobBusy = true;
  if (job == OtaJob::check) {
    manifest.valid = false;
    status.state = OtaState::checking;
    status.error[0] = '\0';
    status.latestVersion[0] = '\0';
    status.canInstall = false;
    status.reinstall = false;
    status.expectedBytes = 0;
    status.downloadedBytes = 0;
  }
  xSemaphoreGive(statusMutex);

  if (xQueueSend(jobQueue, &job, 0) != pdTRUE) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    jobBusy = false;
    xSemaphoreGive(statusMutex);
    return false;
  }
  return true;
}

}  // namespace

void otaBegin(SemaphoreHandle_t networkMutex) {
  sharedNetworkMutex = networkMutex;
  statusMutex = xSemaphoreCreateMutex();
  if (statusMutex == nullptr) {
    Serial.println("[OTA] Could not create status mutex");
    return;
  }
  strlcpy(
      status.installedVersion,
      kFirmwareVersion,
      sizeof(status.installedVersion));
  jobQueue = xQueueCreate(2, sizeof(OtaJob));
  if (jobQueue == nullptr) {
    Serial.println("[OTA] Could not create worker queue");
    return;
  }
  if (xTaskCreatePinnedToCore(
          worker,
          "ota-update",
          12288,
          nullptr,
          1,
          nullptr,
          0) != pdPASS) {
    Serial.println("[OTA] Could not start worker");
    vQueueDelete(jobQueue);
    jobQueue = nullptr;
  }
}

bool otaRequestCheck() {
  return queueJob(OtaJob::check);
}

bool otaRequestInstall() {
  return queueJob(OtaJob::install);
}

OtaStatus otaSnapshot() {
  if (statusMutex == nullptr) {
    OtaStatus unavailable;
    strlcpy(
        unavailable.installedVersion,
        kFirmwareVersion,
        sizeof(unavailable.installedVersion));
    strlcpy(
        unavailable.error,
        "OTA service is unavailable",
        sizeof(unavailable.error));
    unavailable.state = OtaState::error;
    return unavailable;
  }
  OtaStatus snapshot;
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  snapshot = status;
  xSemaphoreGive(statusMutex);
  return snapshot;
}
