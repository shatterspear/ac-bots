-- Cross-faction mail, who list, friends, emotes/chat, channels, and trade are
-- controlled by RBAC in current AzerothCore versions. Grant these permissions
-- to the security-level-0 player role so they apply to every normal account.
INSERT IGNORE INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(195, 25), -- Two-side chat and emotes
(195, 26), -- Two-side channels
(195, 27), -- Two-side mail
(195, 28), -- Two-side who list
(195, 29), -- Add friends from the other faction
(195, 51); -- Trade with the other faction
