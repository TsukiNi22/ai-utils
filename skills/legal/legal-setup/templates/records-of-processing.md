# Records of processing activities (GDPR art. 30) - {{ORGANISATION}}

_Updated {{DATE}} · controller {{NAME, address}} · contact / DPO {{CONTACT}}_

## Processings
| # | Processing | Purposes | Basis | People | Data (special? art. 9) | Recipients | Transfers (safeguard) | Retention | Security |
|---|---|---|---|---|---|---|---|---|---|
| 1 | {{User accounts}} | {{run the service}} | contract | {{users}} | {{email, name, password hash}} | {{host}} | {{none}} | {{account + 30 d}} | {{TLS, hashing, access control}} |
<!-- one row per purpose of the inventory; a DPIA column when one is needed (art. 35, CNIL list) -->

## Processors and sub-processors (art. 28)
| Processor | Service | Data | Country | DPA signed (date / link) | DPF / SCCs |
|---|---|---|---|---|---|
| {{OVHcloud}} | {{hosting}} | {{all}} | {{FR}} | {{yes, DATE}} | {{n/a}} |
<!-- every SDK / API / infra service found in the code; DPA checklist: art. 28(3) a-h, sub-processors, deletion at the end, audits -->

## As a processor for clients {{if any}}
| Client | Processing | Data | Sub-processors | Transfers | Security |
|---|---|---|---|---|---|
