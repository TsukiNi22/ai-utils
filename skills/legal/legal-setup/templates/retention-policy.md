# Retention policy - {{SERVICE_NAME}}

_Updated {{DATE}}. Each period has a reason; at its end the data is deleted or truly anonymised (not just pseudonymised)._

| Data | Active base | Then (intermediate archive, restricted access) | Reason / text | How it is purged |
|---|---|---|---|---|
| {{Accounts}} | {{while open}} | {{30 days (undo), then deleted}} | {{contract}} | {{job `purge_accounts` daily}} |
| {{Server / access logs (IP)}} | {{12 months}} | - | {{security; LCEN décret 2021-1362 for hosts}} | {{log rotation}} |
| {{Prospects (newsletter)}} | {{3 years after the last contact}} | - | {{CNIL référentiel}} | {{job}} |
| {{Invoices}} | {{-}} | {{10 years}} | {{Code de commerce L123-22}} | {{archive then delete}} |
| {{Backups}} | {{rolling N days}} | - | {{restore}} | {{erased data disappears from the backups after N days}} |
| {{Cookies / consent}} | {{13 months / 6 months}} | - | {{CNIL}} | {{expiry}} |
