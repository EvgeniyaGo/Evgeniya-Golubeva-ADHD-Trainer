type GameModeCardProps = {
  title: string;
  subtitle: string;
  label: string;
  icon: "play" | "n";
};

export function GameModeCard({ title, subtitle, label, icon }: GameModeCardProps) {
  return (
    <button className="game-mode-card" type="button">
      <span className={`game-mode-card__icon game-mode-card__icon--${icon}`}>
        {icon === "play" ? "" : "N"}
      </span>
      <span className="game-mode-card__copy">
        <span className="game-mode-card__label">{label}</span>
        <span className="game-mode-card__title">{title}</span>
        <span className="game-mode-card__subtitle">{subtitle}</span>
      </span>
      <span className="game-mode-card__chevron">&gt;</span>
    </button>
  );
}
