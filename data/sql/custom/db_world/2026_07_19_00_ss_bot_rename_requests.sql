CREATE TABLE IF NOT EXISTS `ss_bot_rename_requests` (
  `id` int NOT NULL AUTO_INCREMENT,
  `bot` int NOT NULL,
  `character` int NOT NULL,
  `new_name` text COLLATE utf8mb4_general_ci NOT NULL,
  `status` text COLLATE utf8mb4_general_ci NOT NULL,
  `requested_by` int NOT NULL,
  `requested_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `old_name` text COLLATE utf8mb4_general_ci NOT NULL,
  `admin_comment` text COLLATE utf8mb4_general_ci,
  `status_changed_at` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
