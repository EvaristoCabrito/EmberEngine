export const GOLD_ICON = "/game/icons/gold-coins-pile.png";

export function GoldAmount({ amount, prefix = false, className = "" }: { amount: number; prefix?: boolean; className?: string }) {
  return (
    <span className={`inline-flex items-center gap-1.5 tabular-nums ${className}`.trim()} aria-label={`${amount} Gold`}>
      <img src={GOLD_ICON} alt="" className="size-6 shrink-0 object-contain" />
      <span>{prefix ? "+" : ""}{amount}</span>
      <span>Gold</span>
    </span>
  );
}
