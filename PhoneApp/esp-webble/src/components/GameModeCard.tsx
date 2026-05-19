import { Link } from "react-router-dom";

type GameModeCardProps = {
  title: string;
  subtitle: string;
  label: string;
  icon: "play" | "n" | "snake";
  to?: string;
};

export function GameModeCard({
  title,
  subtitle,
  label,
  icon,
  to,
}: GameModeCardProps) {
  const content = (
    <>
      <span className={`game-mode-card__icon game-mode-card__icon--${icon}`}>
        {icon === "play" ? "" : icon === "snake" ? "3D" : "N"}
      </span>
      <span className="game-mode-card__copy">
        <span className="game-mode-card__label">{label}</span>
        <span className="game-mode-card__title">{title}</span>
        <span className="game-mode-card__subtitle">{subtitle}</span>
      </span>
      <span className="game-mode-card__chevron">&gt;</span>
    </>
  );

  if (to) {
    return (
      <Link className="game-mode-card" to={to}>
        {content}
      </Link>
    );
  }

  return (
    <button className="game-mode-card" type="button">
      {content}
    </button>
  );
}
